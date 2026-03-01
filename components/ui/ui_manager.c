#include "ui_manager.h"
#include "app_config.h"
#include "board.h"
#include "ft6336u.h"
#include "axp192.h"
#include "vibration.h"
#include "ui_compat.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

/* LVGL 觸控讀取回調 */
static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static bool s_last_pressed = false;
    touch_point_t pt;
    ft6336u_read(&pt);
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

/* 各頁面的 lv_obj */
static lv_obj_t *s_screens[5] = {NULL};

/* 前向宣告各頁面初始化 */
extern lv_obj_t *screen_dashboard_create(void);
extern lv_obj_t *screen_ai_analysis_create(void);
extern lv_obj_t *screen_schedule_create(void);
extern lv_obj_t *screen_wifi_create(void);
extern lv_obj_t *screen_settings_create(void);

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
    s_screens[SCREEN_AI_ANALYSIS] = screen_ai_analysis_create();
    s_screens[SCREEN_SCHEDULE]    = screen_schedule_create();
    s_screens[SCREEN_WIFI]        = screen_wifi_create();
    s_screens[SCREEN_SETTINGS]    = screen_settings_create();

    /* 顯示 Dashboard */
    lv_scr_load(s_screens[SCREEN_DASHBOARD]);

    /* 在頂層建立共用 widgets（覆蓋所有頁面）*/
    extern void status_bar_create_on(lv_obj_t *parent);
    extern void loading_spinner_create(lv_obj_t *parent);
    status_bar_create_on(lv_layer_top());
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
    if (id >= 5) return;
    s_cur_screen = id;
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_scr_load_anim(s_screens[id], LV_SCR_LOAD_ANIM_SLIDE_LEFT, 200, 0, false);
        xSemaphoreGiveRecursive(s_ui_mutex);
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

void ui_manager_update_ai_result(const ai_analysis_result_t *result)
{
    extern void screen_ai_analysis_update(const ai_analysis_result_t *r);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        screen_ai_analysis_update(result);
        xSemaphoreGiveRecursive(s_ui_mutex);
    }
}

void ui_manager_update_wifi_state(int state, const char *ip)
{
    extern void status_bar_update_wifi(int state, const char *ip);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        status_bar_update_wifi(state, ip);
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
