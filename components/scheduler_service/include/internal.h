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

extern schedule_config_t s_config;
extern QueueHandle_t     s_quote_queue;
extern TimerHandle_t     s_quote_timer;
extern TaskHandle_t      s_scheduler_task;
extern bool              s_sntp_synced;
extern bool              s_quote_polling_paused;
extern volatile bool     s_quote_fetch_in_flight;
extern stock_list_t      s_stock_list;

void scheduler_service_publish_scheduler_tick(bool force_fetch);
void scheduler_service_publish_quote_fetch_round(bool force_fetch,
                                                 app_quote_fetch_reason_t reason,
                                                 uint8_t total,
                                                 uint8_t pushed,
                                                 uint8_t skipped_empty,
                                                 uint8_t skipped_invalid,
                                                 esp_err_t fetch_err);

bool scheduler_service_is_wifi_connected(void);
void scheduler_service_init_sntp(void);
void scheduler_service_do_fetch_quotes(bool force_fetch);
