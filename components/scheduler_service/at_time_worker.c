#include "internal.h"
#include "ai_provider.h"
#include "telegram_bot.h"
#include "storage.h"
#include "app_config.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "at_time";
static const char *AT_TIME_FIXED_GLOBAL_PROMPT = "*簡短回覆，在320字以內";

static void notify_at_time_failure(scheduler_service_ctx_t *ctx, uint8_t idx, const char *reason)
{
    if (!ctx || idx >= MAX_AT_TIME_COUNT) return;

    uint32_t now_ms = (uint32_t)esp_log_timestamp();
    uint32_t last_ms = ctx->at_time_fail_notify_ms[idx];
    if (last_ms != 0 && (now_ms - last_ms) < AT_TIME_FAIL_NOTIFY_WINDOW_MS) {
        ESP_LOGW(TAG, "entry %u notify throttled (%s)", (unsigned)idx, reason ? reason : "unknown");
        return;
    }

    ctx->at_time_fail_notify_ms[idx] = now_ms;

    char msg[192];
    snprintf(msg, sizeof(msg),
             "[AtTime] Entry %u failed: %s",
             (unsigned)(idx + 1),
             reason ? reason : "unknown");

    esp_err_t tg_ret = telegram_bot_send_text(msg);
    if (tg_ret != ESP_OK) {
        ESP_LOGW(TAG, "entry %u fail notify telegram failed: %s",
                 (unsigned)idx, esp_err_to_name(tg_ret));
    }
}

static void fire_at_time_entry(scheduler_service_ctx_t *ctx, uint8_t idx)
{
    const uint32_t net_drain_timeout_ms = 20000;
    bool should_resume_polling = false;
    bool was_polling_paused = false;
    char *global_prompt = NULL;
    char *combined = NULL;
    char *api_key = NULL;
    ai_analysis_result_t *result = NULL;

    if (!ctx || !ctx->at_time_entries || idx >= ctx->at_time_count) {
        return;
    }

    at_time_entry_t *entry = &ctx->at_time_entries[idx];

    if (telegram_bot_is_running()) {
        was_polling_paused = telegram_bot_is_polling_paused();
        if (!was_polling_paused) {
            telegram_bot_pause_polling();
            should_resume_polling = true;
        }
        esp_err_t idle_ret = telegram_bot_wait_http_idle(net_drain_timeout_ms);
        if (idle_ret != ESP_OK) {
            ESP_LOGW(TAG, "entry %u wait telegram idle timeout", (unsigned)idx);
            goto cleanup;
        }
    }

    /* build combined prompt: global_prompt + "\n" + entry prompt */
    global_prompt = calloc(1, AT_TIME_PROMPT_MAX_LEN + 1);
    if (!global_prompt) {
        ESP_LOGE(TAG, "entry %u global_prompt OOM", (unsigned)idx);
        goto cleanup;
    }
    storage_ai_load_global_prompt(global_prompt, AT_TIME_PROMPT_MAX_LEN + 1);

    size_t gp_len = strlen(global_prompt);
    size_t ep_len = strlen(entry->prompt);
    size_t fp_len = strlen(AT_TIME_FIXED_GLOBAL_PROMPT);
    size_t combined_len = gp_len + ep_len + fp_len + 4; /* up to 3 newlines + NUL */
    combined = malloc(combined_len);
    if (!combined) {
        ESP_LOGE(TAG, "entry %u fire OOM", (unsigned)idx);
        goto cleanup;
    }

    size_t off = 0;
    if (gp_len > 0) {
        memcpy(combined + off, global_prompt, gp_len);
        off += gp_len;
        combined[off++] = '\n';
    }
    if (ep_len > 0) {
        memcpy(combined + off, entry->prompt, ep_len);
        off += ep_len;
        combined[off++] = '\n';
    }
    memcpy(combined + off, AT_TIME_FIXED_GLOBAL_PROMPT, fp_len);
    off += fp_len;
    combined[off] = '\0';

    if (combined[0] == '\0') {
        ESP_LOGW(TAG, "entry %u has empty prompt, skip", (unsigned)idx);
        goto cleanup;
    }

    api_key = calloc(1, 128);
    if (!api_key) {
        ESP_LOGE(TAG, "entry %u api_key OOM", (unsigned)idx);
        goto cleanup;
    }
    ai_provider_get_active_api_key(api_key, 128);

    result = calloc(1, sizeof(ai_analysis_result_t));
    if (!result) {
        ESP_LOGE(TAG, "entry %u result OOM", (unsigned)idx);
        goto cleanup;
    }
    ESP_LOGI(TAG, "entry %u AI prompt len=%u", (unsigned)idx, (unsigned)strlen(combined));
    ESP_LOGI(TAG, "entry %u firing AI call", (unsigned)idx);
    esp_err_t ret = ai_provider_analyze_prompt_sync(combined, api_key, result);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "entry %u AI failed: call=%s result=%s provider=%s detail=%s",
                 (unsigned)idx,
                 esp_err_to_name(ret),
                 esp_err_to_name(result->error_code),
                 ai_provider_get_name(),
                 result->analysis[0] ? result->analysis : "(none)");
        notify_at_time_failure(ctx, idx, "ai_request_failed");
        goto cleanup;
    }

    if (result->analysis[0] != '\0') {
        ESP_LOGI(TAG, "entry %u AI result ready signal=%d confidence=%d len=%u",
                 (unsigned)idx, (int)result->signal, (int)result->confidence,
                 (unsigned)strlen(result->analysis));
        esp_err_t tg_ret = telegram_bot_send_text(result->analysis);
        if (tg_ret == ESP_OK) {
            ESP_LOGI(TAG, "entry %u telegram sent", (unsigned)idx);
        } else {
            ESP_LOGW(TAG, "entry %u telegram failed: %s", (unsigned)idx, esp_err_to_name(tg_ret));
        }
    } else {
        ESP_LOGW(TAG, "entry %u AI returned empty analysis", (unsigned)idx);
    }

cleanup:
    free(result);
    free(api_key);
    free(combined);
    free(global_prompt);
    if (should_resume_polling) {
        telegram_bot_resume_polling();
    }
}

void scheduler_service_check_at_time(scheduler_service_ctx_t *ctx)
{
    if (!ctx || !ctx->sntp_synced || ctx->at_time_count == 0 ||
        !ctx->at_time_entries) {
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
