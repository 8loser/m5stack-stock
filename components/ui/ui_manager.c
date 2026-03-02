#include "ui_manager.h"
#include "app_config.h"
#include "board.h"
#include "ft6336u.h"
#include "axp192.h"
#include "vibration.h"
#include "ui_compat.h"
#include "screen_log.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_mgr";

/* LVGL 顯示 flush 回調 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                           lv_color_t *color_map)
{
    ili9342c_flush(board_get_panel(),
                   area->x1, area->y1, area->x2, area->y2,
                   color_map);
    lv_disp_flush_ready(drv);
}

/* 前向宣告，定義在後方 */
static void handle_hw_button(uint8_t btn);

/* LVGL 觸控讀取回調 */
static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static bool s_last_pressed = false;
    static bool s_hw_btn_fired = false;
    touch_point_t pt;
    esp_err_t touch_ret = ft6336u_read(&pt);
    if (touch_ret != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        s_last_pressed = false;
        s_hw_btn_fired = false;
        return;
    }

    if (!board_is_screen_on()) {
        data->state = LV_INDEV_STATE_RELEASED;
        s_last_pressed = false;
        s_hw_btn_fired = false;
        return;
    }

    /* 攔截底部虛擬按鍵（y >= TOUCH_BTN_Y_MIN），不傳給 LVGL */
    if (pt.pressed && !s_last_pressed) {
        ESP_LOGI(TAG, "touch down x=%d y=%d", pt.x, pt.y);
    }

    /* 兼容兩種 FT6336U 座標回報：
     * - raw 韌體座標：底部鍵常見 y≈270~279
     * - 已映射面板座標：底部鍵落在 y≈220~239 */
    bool in_bottom_btn_zone = (pt.y >= TOUCH_BTN_Y_MIN) || (pt.y >= (LCD_HEIGHT - 20));
    if (pt.pressed && in_bottom_btn_zone) {
        if (!s_hw_btn_fired) {
            s_hw_btn_fired = true;
            uint8_t btn = (pt.x < TOUCH_BTN_A_X_MAX) ? 0 :
                          (pt.x < TOUCH_BTN_B_X_MAX) ? 1 : 2;
            ESP_LOGI(TAG, "底部虛擬按鍵: btn=%u x=%d y=%d", btn, pt.x, pt.y);
            handle_hw_button(btn);
        }
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    s_hw_btn_fired = false;

    if (pt.pressed) {
        data->state  = LV_INDEV_STATE_PRESSED;
        data->point.x = pt.x;
        data->point.y = pt.y;
        /* 只在按下瞬間震動，避免觸控持續按下時連續震動 */
        if (!s_last_pressed) {
            vibration_haptic();
        }
        s_last_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_last_pressed = false;
    }
}

static SemaphoreHandle_t s_ui_mutex   = NULL;
static lv_disp_t        *s_disp       = NULL;
static screen_id_t       s_cur_screen = SCREEN_DASHBOARD;
static const screen_id_t s_nav_screens[] = {
    SCREEN_DASHBOARD,
    SCREEN_LOG,
    SCREEN_INFO,
    SCREEN_SETTINGS,
    SCREEN_HW_TEST,
};
static const size_t s_nav_screens_count = sizeof(s_nav_screens) / sizeof(s_nav_screens[0]);
static const screen_id_t s_mid_screens[] = {
    SCREEN_DASHBOARD,
    SCREEN_PORTAL,
};
static int s_nav_idx = 0;
static int s_mid_btn_idx = 0;

/* 各頁面的 lv_obj */
static lv_obj_t *s_screens[SCREEN_COUNT] = {NULL};

/* 前向宣告各頁面初始化 */
extern lv_obj_t *screen_dashboard_create(void);
extern lv_obj_t *screen_portal_create(void);
extern lv_obj_t *screen_info_create(void);
extern lv_obj_t *screen_settings_create(void);
extern lv_obj_t *screen_hw_test_create(void);
extern void screen_settings_load(void);

static void ui_manager_log_v(log_tag_t tag, log_level_t level, const char *fmt, va_list ap)
{
    if (!fmt) {
        return;
    }

    char msg[56];
    vsnprintf(msg, sizeof(msg), fmt, ap);
    screen_log_push(tag, level, msg);
}

static void lvgl_task(void *arg)
{
    while (1) {
        if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGiveRecursive(s_ui_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

esp_err_t ui_manager_init(SemaphoreHandle_t ui_mutex)
{
    s_ui_mutex = ui_mutex;
    s_cur_screen = SCREEN_DASHBOARD;
    s_nav_idx = 0;
    s_mid_btn_idx = 0;

    lv_init();

    /* LVGL Display Buffer（放在 PSRAM）*/
    static lv_color_t *buf1 = NULL;
    static lv_color_t *buf2 = NULL;
    size_t buf_size = LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t);

    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        /* fallback to internal RAM */
        buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DEFAULT);
        buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DEFAULT);
        if (!buf1 || !buf2) {
            ESP_LOGE(TAG, "LVGL buffer 分配失敗");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGW(TAG, "LVGL buffer 使用內部 RAM");
    }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2,
                           LCD_WIDTH * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res    = LCD_WIDTH;
    disp_drv.ver_res    = LCD_HEIGHT;
    disp_drv.flush_cb   = lvgl_flush_cb;
    disp_drv.draw_buf   = &draw_buf;
    s_disp = lv_disp_drv_register(&disp_drv);

    /* 觸控輸入 */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    /* LVGL Tick Timer */
    esp_timer_handle_t tick_timer;
    esp_timer_create_args_t timer_args = {
        .callback        = ui_lvgl_tick_cb,
        .name            = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000);

    /* 建立所有頁面 */
    s_screens[SCREEN_DASHBOARD]   = screen_dashboard_create();
    s_screens[SCREEN_PORTAL]      = screen_portal_create();
    s_screens[SCREEN_LOG]         = screen_log_create();
    s_screens[SCREEN_INFO]        = screen_info_create();
    s_screens[SCREEN_SETTINGS]    = screen_settings_create();
    s_screens[SCREEN_HW_TEST]     = screen_hw_test_create();

    /* 顯示 Dashboard */
    lv_scr_load(s_screens[SCREEN_DASHBOARD]);

    /* 在頂層建立共用 widgets（覆蓋所有頁面）*/
    extern void status_bar_create_on(lv_obj_t *parent);
    extern void status_bar_set_page(screen_id_t page);
    extern void loading_spinner_create(lv_obj_t *parent);
    status_bar_create_on(lv_layer_top());
    status_bar_set_page(SCREEN_DASHBOARD);
    loading_spinner_create(lv_layer_top());

    /* 啟動 LVGL 任務 */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", STACK_LVGL, NULL,
                             TASK_PRIO_LVGL, NULL, 1);

    ESP_LOGI(TAG, "LVGL UI 初始化完成");
    return ESP_OK;
}

void ui_lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void ui_manager_switch_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT) return;
    if (s_screens[id] == NULL) return;
    screen_id_t prev = s_cur_screen;
    ESP_LOGI(TAG, "[%u ms] switch_screen %d -> %d",
             (unsigned)esp_log_timestamp(), (int)prev, (int)id);

    /* 離開 Portal 頁面時關閉 provisioning portal（含 SoftAP） */
    if (prev == SCREEN_PORTAL && id != SCREEN_PORTAL) {
        extern void screen_portal_close_portal(void);
        screen_portal_close_portal();
    }
    s_cur_screen = id;
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        extern void status_bar_set_page(screen_id_t page);
        status_bar_set_page(id);
        if (id == SCREEN_DASHBOARD) {
            extern void screen_dashboard_refresh(void);
            screen_dashboard_refresh();
        }
        if (id == SCREEN_LOG) {
            screen_log_refresh();
        }
        if (id == SCREEN_INFO) {
            extern void screen_info_refresh(void);
            screen_info_refresh();
        }
        if (id == SCREEN_SETTINGS) {
            screen_settings_load();
        }
        lv_scr_load_anim(s_screens[id], LV_SCR_LOAD_ANIM_SLIDE_LEFT, 200, 0, false);
        xSemaphoreGiveRecursive(s_ui_mutex);
    }

    /* 進入 Portal 頁面時自動啟動 provisioning portal（含 SoftAP） */
    if (id == SCREEN_PORTAL && prev != SCREEN_PORTAL) {
        extern void screen_portal_open_portal(void);
        screen_portal_open_portal();
    }

    for (size_t i = 0; i < s_nav_screens_count; i++) {
        if (s_nav_screens[i] == id) {
            s_nav_idx = (int)i;
            break;
        }
    }
    for (size_t i = 0; i < sizeof(s_mid_screens) / sizeof(s_mid_screens[0]); i++) {
        if (s_mid_screens[i] == id) {
            s_mid_btn_idx = (int)i;
            break;
        }
    }
}

static void handle_hw_button(uint8_t btn)
{
    ESP_LOGI(TAG, "handle_hw_button: cur=%d btn=%u", (int)s_cur_screen, btn);

    if (s_cur_screen == SCREEN_PORTAL && (btn == 0 || btn == 2)) {
        if (btn <= 2) {
            ui_manager_switch_screen(SCREEN_DASHBOARD);
        }
        return;
    }

    if (btn == 1) {
        /* 還原為 Portal 觸發路徑，先驗證 APSTA 穩定性 */
        if (s_cur_screen != SCREEN_PORTAL) {
            ui_manager_switch_screen(SCREEN_PORTAL);
            return;
        }
        ui_manager_switch_screen(SCREEN_DASHBOARD);
        return;
    }

    if (btn == 0 && s_nav_screens_count > 0) {
        s_nav_idx = (s_nav_idx - 1 + (int)s_nav_screens_count) % (int)s_nav_screens_count;
        ui_manager_switch_screen(s_nav_screens[s_nav_idx]);
        return;
    }

    if (btn == 2 && s_nav_screens_count > 0) {
        s_nav_idx = (s_nav_idx + 1) % (int)s_nav_screens_count;
        ui_manager_switch_screen(s_nav_screens[s_nav_idx]);
    }
}

screen_id_t ui_manager_get_current_screen(void)
{
    return s_cur_screen;
}

void ui_manager_update_quote(const stock_quote_t *quote)
{
    extern void screen_dashboard_update(const stock_quote_t *q);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        screen_dashboard_update(quote);
        xSemaphoreGiveRecursive(s_ui_mutex);
    }
}

void ui_manager_set_dashboard_card_count(uint8_t n)
{
    extern void screen_dashboard_set_card_count(uint8_t n);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        screen_dashboard_set_card_count(n);
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "set_dashboard_card_count timeout, n=%u", (unsigned)n);
    }
}

void ui_manager_update_wifi_state(int state, const char *ip)
{
    extern void status_bar_update_wifi(int state, const char *ip);
    extern void screen_portal_refresh_network_info(void);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        status_bar_update_wifi(state, ip);
        if (s_cur_screen == SCREEN_PORTAL) {
            screen_portal_refresh_network_info();
        }
        xSemaphoreGiveRecursive(s_ui_mutex);
    }
}

void ui_manager_show_loading(bool show)
{
    extern void loading_spinner_set_visible(bool v);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        loading_spinner_set_visible(show);
        xSemaphoreGiveRecursive(s_ui_mutex);
    }
}

void ui_manager_log_stock(log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ui_manager_log_v(LOG_TAG_STOCK, level, fmt, ap);
    va_end(ap);
}

void ui_manager_log_wifi(log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ui_manager_log_v(LOG_TAG_WIFI, level, fmt, ap);
    va_end(ap);
}

void ui_manager_log_sys(log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ui_manager_log_v(LOG_TAG_SYS, level, fmt, ap);
    va_end(ap);
}
