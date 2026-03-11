#pragma once

#include "app_event_bus.h"
#include "scheduler_service.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdbool.h>
#include <stdint.h>

#define SCHEDULER_SERVICE_NOTIFY_QUOTE_BIT       (1U << 0)
#define SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT (1U << 1)
#define SCHEDULER_SERVICE_NOTIFY_CMD_BIT         (1U << 2)

typedef enum {
    SCHED_CMD_TEST_AI_KEY,
    SCHED_CMD_TEST_STOCK_ALERT,
} sched_cmd_type_t;

typedef struct {
    sched_cmd_type_t type;
    void            *result;          /* 指向呼叫端的結果 struct */
    SemaphoreHandle_t done;           /* 完成信號 */
    /* --- CMD_TEST_AI_KEY params --- */
    int              selected_provider;
    char             api_key[128];    /* 使用者提供的 key（空字串=用 NVS） */
    char             symbol[8];
    stock_quote_t    quote;
    stock_alert_config_t alert_cfg;
} sched_cmd_item_t;

typedef struct {
    bool  active;
    bool  enabled;
    bool  latched;
    char  symbol[8];
    float threshold_pct;
} stock_alert_state_t;

typedef struct {
    stock_quote_t quote;
    bool          trigger_up;
    float         threshold_pct;
} stock_alert_event_t;

typedef struct {
    schedule_config_t config;
    QueueHandle_t     quote_queue;
    QueueHandle_t     cmd_queue;
    QueueHandle_t     stock_alert_queue;
    TimerHandle_t     quote_timer;
    TaskHandle_t      scheduler_task;
    TaskHandle_t      stock_alert_task;
    SemaphoreHandle_t alert_mutex;
    volatile bool     stock_alert_in_flight;
    bool              sntp_synced;
    bool              quote_polling_paused;
    volatile bool     quote_fetch_in_flight;
    stock_list_t      stock_list;
    stock_alert_state_t alert_states[MAX_STOCK_COUNT];
    /* --- AtTime (heap-allocated to save ~4KB .bss) --- */
    at_time_entry_t  *at_time_entries;
    uint8_t           at_time_count;
    uint8_t           at_time_fired_bitmask;
    int8_t            at_time_prev_minute;
    uint32_t          at_time_fail_notify_ms[MAX_AT_TIME_COUNT];
} scheduler_service_ctx_t;

scheduler_service_ctx_t *scheduler_service_ctx(void);

void scheduler_service_publish_scheduler_tick(const scheduler_service_ctx_t *ctx,
                                              bool force_fetch);
void scheduler_service_publish_quote_fetch_round(const scheduler_service_ctx_t *ctx,
                                                 bool force_fetch,
                                                 app_quote_fetch_reason_t reason,
                                                 uint8_t total,
                                                 uint8_t pushed,
                                                 uint8_t skipped_empty,
                                                 uint8_t skipped_invalid,
                                                 esp_err_t fetch_err);

bool scheduler_service_is_wifi_connected(void);
void scheduler_service_init_sntp(scheduler_service_ctx_t *ctx);
void scheduler_service_do_fetch_quotes(scheduler_service_ctx_t *ctx, bool force_fetch);
void scheduler_service_check_at_time(scheduler_service_ctx_t *ctx);
void scheduler_service_process_cmd_queue(scheduler_service_ctx_t *ctx);
esp_err_t scheduler_service_stock_alert_init(scheduler_service_ctx_t *ctx);
esp_err_t scheduler_service_stock_alert_reload_configs(scheduler_service_ctx_t *ctx);
void scheduler_service_stock_alert_on_quote(scheduler_service_ctx_t *ctx, const stock_quote_t *quote);
esp_err_t scheduler_service_stock_alert_trigger_test(scheduler_service_ctx_t *ctx,
                                                     const stock_quote_t *quote,
                                                     const stock_alert_config_t *cfg,
                                                     sched_stock_alert_test_result_t *result);
