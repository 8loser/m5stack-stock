#include "internal.h"
#include "esp_log.h"

static const char *TAG = "scheduler";

void scheduler_service_publish_scheduler_tick(bool force_fetch)
{
    app_event_t evt = {
        .type = APP_EVENT_SCHEDULER_TICK,
        .timestamp_ms = 0,
    };
    evt.data.scheduler.force_fetch = force_fetch;
    if (app_event_publish(&evt) != ESP_OK) {
        ESP_LOGW(TAG, "publish scheduler tick failed");
    }
}

void scheduler_service_publish_quote_fetch_round(bool force_fetch,
                                                 app_quote_fetch_reason_t reason,
                                                 uint8_t total,
                                                 uint8_t pushed,
                                                 uint8_t skipped_empty,
                                                 uint8_t skipped_invalid,
                                                 esp_err_t fetch_err)
{
    app_event_t evt = {
        .type = APP_EVENT_QUOTE_FETCH_ROUND,
        .timestamp_ms = 0,
    };
    evt.data.quote_fetch_round.force_fetch = force_fetch;
    evt.data.quote_fetch_round.total = total;
    evt.data.quote_fetch_round.pushed = pushed;
    evt.data.quote_fetch_round.skipped_empty = skipped_empty;
    evt.data.quote_fetch_round.skipped_invalid = skipped_invalid;
    evt.data.quote_fetch_round.fetch_err = fetch_err;
    evt.data.quote_fetch_round.reason = reason;

    if (app_event_publish(&evt) != ESP_OK) {
        ESP_LOGW(TAG, "publish quote fetch round failed");
    }
}
