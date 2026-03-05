#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "screen_log.h"
#include "twse_models.h"
#include "ui_compat.h"

typedef enum {
    SCREEN_DASHBOARD    = 0,
    SCREEN_PORTAL       = 1,
    SCREEN_LOG          = 2,
    SCREEN_INFO         = 3,
    SCREEN_SETTINGS     = 4,
    SCREEN_HW_TEST      = 5,
    SCREEN_COUNT
} screen_id_t;

esp_err_t ui_manager_init(SemaphoreHandle_t ui_mutex);
void      ui_manager_switch_screen(screen_id_t id);
screen_id_t ui_manager_get_current_screen(void);

/* 資料更新（從 queue 消費後呼叫）*/
void ui_manager_update_quote(const stock_quote_t *quote);
void ui_manager_update_wifi_state(int state, const char *ip);
void ui_manager_show_loading(bool show);
void ui_manager_set_dashboard_card_count(uint8_t n);
void ui_manager_set_dashboard_symbols(const char symbols[][8], uint8_t count);
void ui_manager_heartbeat_feed_main(void);
void ui_manager_heartbeat_feed_scheduler(void);
bool ui_manager_is_main_flow_alive(uint32_t *age_main_ms, uint32_t *age_sched_ms);
void ui_manager_set_startup_ready(bool ready);
bool ui_manager_is_startup_guard_active(void);
void ui_manager_log_stock(log_level_t level, const char *fmt, ...);
void ui_manager_log_wifi(log_level_t level, const char *fmt, ...);
void ui_manager_log_sys(log_level_t level, const char *fmt, ...);

/* LVGL tick（由 esp_timer 呼叫）*/
void ui_lvgl_tick_cb(void *arg);
