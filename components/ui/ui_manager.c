#include "ui_manager.h"
#include "app_config.h"
#include "board.h"
#include "ft6336u.h"
#include "axp192.h"
#include "vibration.h"
#include "ui_compat.h"
#include "screen_log.h"
#include "screen_dashboard.h"
#include "screen_boot.h"
#include "screen_portal.h"
#include "screen_info.h"
#include "screen_settings.h"
#include "screen_hw_test.h"
#include "status_bar.h"
#include "scheduler_service.h"
#include "telegram_bot.h"
#include "network_portal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_mgr";
static uint32_t    s_last_touch_ms = 0;
static uint32_t    s_last_auto_return_ms = 0;
static bool        s_last_screen_on = true;
static bool        s_lcd_flush_async_ready = false;
static SemaphoreHandle_t s_lcd_flush_done_sem = NULL;
static volatile uint32_t s_lcd_flush_submit_count = 0;
static volatile uint32_t s_lcd_flush_done_count = 0;
static volatile uint32_t s_lcd_flush_timeout_count = 0;

static bool lvgl_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    BaseType_t high_task_wakeup = pdFALSE;
    SemaphoreHandle_t done_sem = (SemaphoreHandle_t)user_ctx;
    if (done_sem != NULL) {
        s_lcd_flush_done_count++;
        if (xPortInIsrContext()) {
            xSemaphoreGiveFromISR(done_sem, &high_task_wakeup);
        } else {
            xSemaphoreGive(done_sem);
        }
    }
    return high_task_wakeup == pdTRUE;
}

/* LVGL 顯示 flush 回調 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                           lv_color_t *color_map)
{
    if (s_lcd_flush_async_ready && s_lcd_flush_done_sem != NULL) {
        /* Drain stale signal before queueing a new flush transaction. */
        (void)xSemaphoreTake(s_lcd_flush_done_sem, 0);
    }

    esp_err_t ret = ili9342c_flush(board_get_panel(),
                                   area->x1, area->y1, area->x2, area->y2,
                                   color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD flush failed: %s", esp_err_to_name(ret));
        lv_disp_flush_ready(drv);
        return;
    }
    s_lcd_flush_submit_count++;

    if (s_lcd_flush_async_ready && s_lcd_flush_done_sem != NULL) {
        /* Wait for completion of the flush just queued above. */
        if (xSemaphoreTake(s_lcd_flush_done_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
            s_lcd_flush_timeout_count++;
            ESP_LOGW(TAG, "LCD flush done wait timeout submit=%lu done=%lu timeout=%lu",
                     (unsigned long)s_lcd_flush_submit_count,
                     (unsigned long)s_lcd_flush_done_count,
                     (unsigned long)s_lcd_flush_timeout_count);
            /* Timeout can be transient under bus contention; wait longer before giving up. */
            if (xSemaphoreTake(s_lcd_flush_done_sem, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGE(TAG, "LCD flush fallback wait failed submit=%lu done=%lu",
                         (unsigned long)s_lcd_flush_submit_count,
                         (unsigned long)s_lcd_flush_done_count);
            }
        }
    }
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
    if (pt.pressed) {
        s_last_touch_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
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
static screen_id_t       s_cur_screen = SCREEN_BOOT;
static portMUX_TYPE      s_heartbeat_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE      s_boot_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t          s_main_heartbeat_ms = 0;
static uint32_t          s_scheduler_heartbeat_ms = 0;
static bool              s_boot_active = true;
static bool              s_status_bar_show_pending = false;

#define MAIN_HEARTBEAT_TIMEOUT_MS      1500U
#define SCHED_HEARTBEAT_TIMEOUT_MS     3000U
#define PORTAL_NET_DRAIN_TIMEOUT_MS    (HTTP_TIMEOUT_MS + 5000U)
#define PORTAL_TG_DRAIN_TIMEOUT_MS     PORTAL_NET_DRAIN_TIMEOUT_MS
#define AUTO_RETURN_GUARD_MS           500U
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

static bool ui_can_open_dashboard(void)
{
    return network_portal_is_connected();
}

static screen_id_t ui_get_home_screen(void)
{
    return ui_can_open_dashboard() ? SCREEN_DASHBOARD : SCREEN_PORTAL;
}

static void ui_switch_nav_step(int step)
{
    int base_idx = s_nav_idx;
    int idx = 0;

    for (idx = 0; idx < (int)s_nav_screens_count; idx++) {
        if (s_nav_screens[idx] == s_cur_screen) {
            base_idx = idx;
            break;
        }
    }

    idx = base_idx;
    for (int i = 0; i < (int)s_nav_screens_count; i++) {
        idx = (idx + step + (int)s_nav_screens_count) % (int)s_nav_screens_count;
        if (s_nav_screens[idx] == SCREEN_DASHBOARD && !ui_can_open_dashboard()) {
            continue;
        }
        s_nav_idx = idx;
        ui_manager_switch_screen(s_nav_screens[idx]);
        return;
    }
}

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
    uint32_t last_diag_ms = 0;
    while (1) {
        if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_task_handler();
            bool show_pending = false;
            taskENTER_CRITICAL(&s_boot_lock);
            show_pending = s_status_bar_show_pending;
            if (show_pending) {
                s_status_bar_show_pending = false;
            }
            taskEXIT_CRITICAL(&s_boot_lock);
            if (show_pending) {
                status_bar_set_visible(true);
            }
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

            bool screen_on = board_is_screen_on();
            if (s_last_screen_on != screen_on) {
                now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
                if (s_last_screen_on && !screen_on) {
                    s_last_touch_ms = now_ms;
                    s_last_auto_return_ms = now_ms;
                    if (s_cur_screen != SCREEN_DASHBOARD && s_cur_screen != SCREEN_BOOT) {
                        screen_id_t home = ui_get_home_screen();
                        ESP_LOGI(TAG, "screen off, preload home=%d", (int)home);
                        ui_manager_switch_screen(home);
                    }
                } else if (!s_last_screen_on && screen_on) {
                    /* 亮屏只重置基準，避免沿用熄屏前時間 */
                    s_last_touch_ms = now_ms;
                    s_last_auto_return_ms = now_ms;
                }
            }
            s_last_screen_on = screen_on;

            if (screen_on &&
                s_cur_screen != SCREEN_DASHBOARD &&
                s_cur_screen != SCREEN_PORTAL &&
                s_cur_screen != SCREEN_BOOT) {
                uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
                if ((now_ms - s_last_touch_ms) >= UI_NON_HOME_IDLE_RETURN_MS &&
                    (now_ms - s_last_auto_return_ms) >= AUTO_RETURN_GUARD_MS) {
                    screen_id_t home = ui_get_home_screen();
                    s_last_auto_return_ms = now_ms;
                    ESP_LOGI(TAG, "idle timeout, return to home=%d", (int)home);
                    ui_manager_switch_screen(home);
                }
            }
            xSemaphoreGiveRecursive(s_ui_mutex);
        }
        uint32_t now_ms = (uint32_t)esp_log_timestamp();
        if (now_ms - last_diag_ms >= 30000U) {
            UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "diag stack_hwm lvgl=%u", (unsigned)wm);
            last_diag_ms = now_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

esp_err_t ui_manager_init(SemaphoreHandle_t ui_mutex)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_ui_mutex = ui_mutex;
    s_cur_screen = SCREEN_BOOT;
    s_nav_idx = 0;
    s_mid_btn_idx = 0;
    s_last_touch_ms = now_ms;
    s_last_auto_return_ms = now_ms;
    s_last_screen_on = board_is_screen_on();
    taskENTER_CRITICAL(&s_boot_lock);
    s_boot_active = true;
    s_status_bar_show_pending = false;
    taskEXIT_CRITICAL(&s_boot_lock);
    taskENTER_CRITICAL(&s_heartbeat_lock);
    s_main_heartbeat_ms = now_ms;
    s_scheduler_heartbeat_ms = now_ms;
    taskEXIT_CRITICAL(&s_heartbeat_lock);

    lv_init();

    /* LVGL Display Buffer（優先 DMA-capable internal RAM，避免 SPI DMA 不穩）*/
    static lv_color_t *buf1 = NULL;
    static lv_color_t *buf2 = NULL;
    size_t buf_size = LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t);

    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        /* fallback to PSRAM when internal DMA memory is insufficient */
        buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        if (!buf1 || !buf2) {
            ESP_LOGE(TAG, "LVGL buffer 分配失敗");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGW(TAG, "LVGL buffer fallback 使用 PSRAM（可能影響 LCD flush 穩定性）");
    } else {
        ESP_LOGI(TAG, "LVGL buffer 使用 DMA internal RAM");
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
    s_lcd_flush_done_sem = xSemaphoreCreateBinary();
    if (s_lcd_flush_done_sem == NULL) {
        s_lcd_flush_async_ready = false;
        ESP_LOGW(TAG, "create LCD flush done semaphore failed, fallback to sync ready");
    } else {
        esp_err_t flush_cb_ret =
            ili9342c_register_flush_done_callback(lvgl_flush_ready_cb, s_lcd_flush_done_sem);
        if (flush_cb_ret == ESP_OK) {
            s_lcd_flush_async_ready = true;
        } else {
            s_lcd_flush_async_ready = false;
            ESP_LOGW(TAG, "register flush done callback failed, fallback to sync ready: %s",
                     esp_err_to_name(flush_cb_ret));
        }
    }

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
    s_screens[SCREEN_BOOT]        = screen_boot_create();
    s_screens[SCREEN_PORTAL]      = screen_portal_create();
    s_screens[SCREEN_LOG]         = screen_log_create();
    s_screens[SCREEN_INFO]        = screen_info_create();
    s_screens[SCREEN_SETTINGS]    = screen_settings_create();
    s_screens[SCREEN_HW_TEST]     = screen_hw_test_create();

    /* 開機流程先顯示 Boot screen */
    lv_scr_load(s_screens[SCREEN_BOOT]);

    /* 在頂層建立共用 widgets（覆蓋所有頁面）*/
    status_bar_create_on(lv_layer_top());
    status_bar_set_page(SCREEN_BOOT);
    status_bar_set_visible(false);

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
    bool boot_active = false;

    if (id >= SCREEN_COUNT) return;
    if (s_screens[id] == NULL) return;
    taskENTER_CRITICAL(&s_boot_lock);
    boot_active = s_boot_active;
    taskEXIT_CRITICAL(&s_boot_lock);
    if (boot_active && id != SCREEN_BOOT) {
        ESP_LOGI(TAG, "boot active, ignore switch to %d", (int)id);
        return;
    }
    if (id == SCREEN_DASHBOARD && !ui_can_open_dashboard()) {
        ESP_LOGI(TAG, "wifi disconnected, block switch to dashboard");
        return;
    }

    screen_id_t prev = s_cur_screen;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    ESP_LOGI(TAG, "[%u ms] switch_screen %d -> %d",
             (unsigned)esp_log_timestamp(), (int)prev, (int)id);

    /* 離開 Portal 頁面時關閉 provisioning portal */
    if (prev == SCREEN_PORTAL && id != SCREEN_PORTAL) {
        bool was_sta_mode = network_portal_is_portal_sta_mode();
        screen_portal_close_portal();
        if (was_sta_mode) {
            /* STA mode: bot was paused, resume polling */
            telegram_bot_resume_polling();
        } else {
            /* AP mode: bot was stopped, restart it */
            telegram_bot_start();
        }
        esp_err_t resume_ret = scheduler_service_resume_quote_polling();
        if (resume_ret != ESP_OK) {
            ESP_LOGW(TAG, "resume quote polling failed: %s", esp_err_to_name(resume_ret));
        }
    }
    if (prev == SCREEN_DASHBOARD && id != SCREEN_DASHBOARD) {
        screen_dashboard_on_leave();
    }
    s_cur_screen = id;
    if (id != SCREEN_DASHBOARD && id != SCREEN_PORTAL) {
        s_last_touch_ms = now_ms;
    }
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status_bar_set_page(id);
        if (id == SCREEN_DASHBOARD) {
            screen_dashboard_on_enter();
        }
        if (id == SCREEN_LOG) {
            screen_log_refresh();
        }
        if (id == SCREEN_INFO) {
            screen_info_refresh();
        }
        if (id == SCREEN_SETTINGS) {
            screen_settings_load();
        }
        if (id == SCREEN_BOOT || prev == SCREEN_BOOT ||
            id == SCREEN_SETTINGS || id == SCREEN_PORTAL || prev == SCREEN_PORTAL) {
            /* Settings and portal transitions are sensitive to long-running side effects. */
            lv_scr_load(s_screens[id]);
        } else {
            lv_scr_load_anim(s_screens[id], LV_SCR_LOAD_ANIM_SLIDE_LEFT, 200, 0, false);
        }
        xSemaphoreGiveRecursive(s_ui_mutex);
    }

    /* 進入 Portal 頁面時自動啟動 provisioning portal */
    if (id == SCREEN_PORTAL && prev != SCREEN_PORTAL) {
        bool sta_connected = network_portal_is_connected();

        if (sta_connected) {
            /* STA mode: WiFi connected, no AP switch needed.
             * Bot stays alive (paused) for cmd queue, no DRAM pressure. */
            telegram_bot_pause_polling();
            esp_err_t tg_idle_ret = telegram_bot_wait_http_idle(PORTAL_TG_DRAIN_TIMEOUT_MS);
            if (tg_idle_ret != ESP_OK) {
                ESP_LOGW(TAG, "telegram http not idle: %s", esp_err_to_name(tg_idle_ret));
            }
        } else {
            /* AP mode: no WiFi, stop bot to free DRAM for pure AP. */
            telegram_bot_stop();
            esp_err_t tg_wait_ret = telegram_bot_wait_stopped(PORTAL_TG_DRAIN_TIMEOUT_MS);
            if (tg_wait_ret != ESP_OK) {
                ESP_LOGW(TAG, "telegram bot not stopped: %s", esp_err_to_name(tg_wait_ret));
            }
        }

        esp_err_t pause_ret = scheduler_service_pause_quote_polling();
        if (pause_ret != ESP_OK) {
            ESP_LOGW(TAG, "pause quote polling failed: %s", esp_err_to_name(pause_ret));
        }
        esp_err_t sched_idle_ret = scheduler_service_wait_quote_fetch_idle(PORTAL_NET_DRAIN_TIMEOUT_MS);
        if (sched_idle_ret != ESP_OK) {
            ESP_LOGW(TAG, "scheduler fetch not idle: %s", esp_err_to_name(sched_idle_ret));
        }

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
    bool boot_active = false;

    taskENTER_CRITICAL(&s_boot_lock);
    boot_active = s_boot_active;
    taskEXIT_CRITICAL(&s_boot_lock);
    if (boot_active) {
        ESP_LOGI(TAG, "boot active, ignore nav btn=%u", (unsigned)btn);
        return;
    }

    ESP_LOGI(TAG, "handle_hw_button: cur=%d btn=%u", (int)s_cur_screen, btn);

    if (s_cur_screen == SCREEN_PORTAL && (btn == 0 || btn == 2)) {
        ui_switch_nav_step((btn == 0) ? -1 : 1);
        return;
    }

    if (btn == 1) {
        /* 還原為 Portal 觸發路徑，先驗證 APSTA 穩定性 */
        if (s_cur_screen != SCREEN_PORTAL) {
            ui_manager_switch_screen(SCREEN_PORTAL);
            return;
        }
        if (ui_can_open_dashboard()) {
            ui_manager_switch_screen(SCREEN_DASHBOARD);
        } else {
            ESP_LOGI(TAG, "wifi disconnected, keep portal on mid key");
        }
        return;
    }

    if (btn == 0 && s_nav_screens_count > 0) {
        ui_switch_nav_step(-1);
        return;
    }

    if (btn == 2 && s_nav_screens_count > 0) {
        ui_switch_nav_step(1);
    }
}

screen_id_t ui_manager_get_current_screen(void)
{
    return s_cur_screen;
}

void ui_manager_update_quote(const stock_quote_t *quote)
{
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        screen_dashboard_update(quote);
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "update_quote timeout symbol=%s", quote ? quote->symbol : "");
    }
}

void ui_manager_set_dashboard_card_count(uint8_t n)
{
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        screen_dashboard_set_card_count(n);
        status_bar_set_dashboard_watch_count(n);
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "set_dashboard_card_count timeout, n=%u", (unsigned)n);
    }
}

void ui_manager_set_dashboard_symbols(const char symbols[][8], uint8_t count)
{
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        uint8_t watch_count = (count <= MAX_STOCK_COUNT) ? count : MAX_STOCK_COUNT;
        screen_dashboard_set_symbols(symbols, count);
        status_bar_set_dashboard_watch_count(watch_count);
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "set_dashboard_symbols timeout, count=%u", (unsigned)count);
    }
}

void ui_manager_update_wifi_state(int state, const char *ip)
{
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        status_bar_update_wifi(state, ip);
        if (s_cur_screen == SCREEN_PORTAL) {
            screen_portal_refresh_network_info();
        }
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "update_wifi_state timeout state=%d ip=%s", state, ip ? ip : "");
    }
}

void ui_manager_set_boot_progress(const char *text)
{
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        screen_boot_set_progress(text);
        xSemaphoreGiveRecursive(s_ui_mutex);
    }
}

void ui_manager_finish_boot(bool wifi_connected)
{
    bool boot_active = false;
    screen_id_t target = wifi_connected ? SCREEN_DASHBOARD : SCREEN_PORTAL;

    taskENTER_CRITICAL(&s_boot_lock);
    boot_active = s_boot_active;
    s_boot_active = false;
    s_status_bar_show_pending = true;
    taskEXIT_CRITICAL(&s_boot_lock);

    if (!boot_active) {
        return;
    }

    ui_manager_switch_screen(target);
    if (xSemaphoreTakeRecursive(s_ui_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status_bar_set_visible(true);
        taskENTER_CRITICAL(&s_boot_lock);
        s_status_bar_show_pending = false;
        taskEXIT_CRITICAL(&s_boot_lock);
        xSemaphoreGiveRecursive(s_ui_mutex);
    } else {
        ESP_LOGW(TAG, "finish_boot: status bar show deferred (ui_mutex busy)");
    }
}

void ui_manager_heartbeat_feed_main(void)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    taskENTER_CRITICAL(&s_heartbeat_lock);
    s_main_heartbeat_ms = now_ms;
    taskEXIT_CRITICAL(&s_heartbeat_lock);
}

void ui_manager_heartbeat_feed_scheduler(void)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    taskENTER_CRITICAL(&s_heartbeat_lock);
    s_scheduler_heartbeat_ms = now_ms;
    taskEXIT_CRITICAL(&s_heartbeat_lock);
}

bool ui_manager_is_main_flow_alive(uint32_t *age_main_ms, uint32_t *age_sched_ms)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t main_ms = 0;
    uint32_t sched_ms = 0;
    uint32_t main_age = 0;
    uint32_t sched_age = 0;

    taskENTER_CRITICAL(&s_heartbeat_lock);
    main_ms = s_main_heartbeat_ms;
    sched_ms = s_scheduler_heartbeat_ms;
    taskEXIT_CRITICAL(&s_heartbeat_lock);

    main_age = now_ms - main_ms;
    sched_age = now_ms - sched_ms;

    if (age_main_ms) {
        *age_main_ms = main_age;
    }
    if (age_sched_ms) {
        *age_sched_ms = sched_age;
    }

    return (main_age <= MAIN_HEARTBEAT_TIMEOUT_MS) &&
           (sched_age <= SCHED_HEARTBEAT_TIMEOUT_MS);
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
