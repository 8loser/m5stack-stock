#include "internal.h"
#include "ai_provider.h"
#include "telegram_bot.h"
#include "storage.h"
#include "app_config.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

static const char *TAG = "at_time";

static void fire_at_time_entry(scheduler_service_ctx_t *ctx, uint8_t idx)
{
    at_time_entry_t *entry = &ctx->at_time_entries[idx];

    /* build combined prompt: global_prompt + "\n" + entry prompt */
    char global_prompt[AT_TIME_PROMPT_MAX_LEN + 1] = {0};
    storage_ai_load_global_prompt(global_prompt, sizeof(global_prompt));

    size_t gp_len = strlen(global_prompt);
    size_t ep_len = strlen(entry->prompt);
    size_t combined_len = gp_len + 1 + ep_len + 1;
    char *combined = malloc(combined_len);
    if (!combined) {
        ESP_LOGE(TAG, "entry %u fire OOM", (unsigned)idx);
        return;
    }

    if (gp_len > 0 && ep_len > 0) {
        memcpy(combined, global_prompt, gp_len);
        combined[gp_len] = '\n';
        memcpy(combined + gp_len + 1, entry->prompt, ep_len);
        combined[gp_len + 1 + ep_len] = '\0';
    } else if (gp_len > 0) {
        memcpy(combined, global_prompt, gp_len + 1);
    } else {
        memcpy(combined, entry->prompt, ep_len + 1);
    }

    if (combined[0] == '\0') {
        ESP_LOGW(TAG, "entry %u has empty prompt, skip", (unsigned)idx);
        free(combined);
        return;
    }

    char api_key[128] = {0};
    ai_provider_get_active_api_key(api_key, sizeof(api_key));

    ai_analysis_result_t result = {0};
    ESP_LOGI(TAG, "entry %u firing AI call", (unsigned)idx);
    esp_err_t ret = ai_provider_analyze_prompt_sync(combined, api_key, &result);
    free(combined);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "entry %u AI failed: %s", (unsigned)idx, esp_err_to_name(ret));
        return;
    }

    if (result.analysis[0] != '\0') {
        esp_err_t tg_ret = telegram_bot_send_text(result.analysis);
        if (tg_ret == ESP_OK) {
            ESP_LOGI(TAG, "entry %u telegram sent", (unsigned)idx);
        } else {
            ESP_LOGW(TAG, "entry %u telegram failed: %s", (unsigned)idx, esp_err_to_name(tg_ret));
        }
    } else {
        ESP_LOGW(TAG, "entry %u AI returned empty analysis", (unsigned)idx);
    }
}

void scheduler_service_check_at_time(scheduler_service_ctx_t *ctx)
{
    if (!ctx->sntp_synced || ctx->at_time_count == 0 || !ctx->at_time_entries) {
        return;
    }

    time_t now_t = time(NULL);
    struct tm tm_now;
    localtime_r(&now_t, &tm_now);

    int cur_minute = tm_now.tm_min;
    int cur_hour   = tm_now.tm_hour;
    int cur_wday   = tm_now.tm_wday;  /* 0=Sun */

    /* reset fired bitmask on minute change */
    if (ctx->at_time_prev_minute != (int8_t)cur_minute) {
        ctx->at_time_fired_bitmask = 0;
        ctx->at_time_prev_minute = (int8_t)cur_minute;
    }

    for (uint8_t i = 0; i < ctx->at_time_count; i++) {
        at_time_entry_t *e = &ctx->at_time_entries[i];

        if (!e->enabled) continue;
        if (ctx->at_time_fired_bitmask & (1U << i)) continue;
        if (e->hour != (uint8_t)cur_hour) continue;
        if (e->minute != (uint8_t)cur_minute) continue;
        if (!(e->weekdays & (1U << cur_wday))) continue;

        /* avoid HTTP conflict with quote fetch */
        if (ctx->quote_fetch_in_flight) continue;

        ctx->at_time_fired_bitmask |= (1U << i);

        ESP_LOGI(TAG, "AtTime entry %u fired at %02d:%02d wday=%d",
                 (unsigned)i, cur_hour, cur_minute, cur_wday);
        fire_at_time_entry(ctx, i);
    }
}
