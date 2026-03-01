#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "twse_models.h"
#include "ai_provider.h"

typedef enum {
    SCREEN_DASHBOARD    = 0,
    SCREEN_AI_ANALYSIS  = 1,
    SCREEN_SCHEDULE     = 2,
    SCREEN_WIFI         = 3,
    SCREEN_SETTINGS     = 4,
} screen_id_t;

esp_err_t ui_manager_init(SemaphoreHandle_t ui_mutex);
void      ui_manager_switch_screen(screen_id_t id);
screen_id_t ui_manager_get_current_screen(void);

/* 資料更新（從 queue 消費後呼叫）*/
void ui_manager_update_quote(const stock_quote_t *quote);
void ui_manager_update_ai_result(const ai_analysis_result_t *result);
void ui_manager_update_wifi_state(int state, const char *ip);
void ui_manager_show_loading(bool show);

/* LVGL tick（由 esp_timer 呼叫）*/
void ui_lvgl_tick_cb(void *arg);
