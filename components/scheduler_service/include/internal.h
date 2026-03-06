#pragma once

#include "app_event_bus.h"
#include "scheduler_service.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdbool.h>
#include <stdint.h>

#define SCHEDULER_SERVICE_NOTIFY_QUOTE_BIT       (1U << 0)
#define SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT (1U << 1)

typedef struct {
    schedule_config_t config;
    QueueHandle_t     quote_queue;
    TimerHandle_t     quote_timer;
    TaskHandle_t      scheduler_task;
    bool              sntp_synced;
    bool              quote_polling_paused;
    volatile bool     quote_fetch_in_flight;
    stock_list_t      stock_list;
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
