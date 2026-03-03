#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_EVENT_WIFI_STATE_CHANGED = 0,
    APP_EVENT_STOCK_LIST_CHANGED,
    APP_EVENT_QUOTE_UPDATED,
    APP_EVENT_SCHEDULER_TICK,
    APP_EVENT_AI_RESULT_READY,
    APP_EVENT_UI_NAV_REQUEST,
    APP_EVENT_COUNT,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    uint32_t timestamp_ms;
    union {
        struct {
            int state;
            char ip[20];
        } wifi;
        struct {
            uint8_t count;
        } stock_list;
        struct {
            char symbol[8];
        } quote;
        struct {
            bool force_fetch;
        } scheduler;
        struct {
            int signal;
        } ai;
        struct {
            int screen_id;
        } ui_nav;
    } data;
} app_event_t;

typedef void (*app_event_cb_t)(const app_event_t *evt, void *ctx);

esp_err_t app_event_bus_init(void);
esp_err_t app_event_publish(const app_event_t *evt);
esp_err_t app_event_subscribe(app_event_type_t type, app_event_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
