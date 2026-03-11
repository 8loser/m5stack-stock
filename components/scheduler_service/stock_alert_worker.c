#include "internal.h"
#include "ai_provider.h"
#include "telegram_bot.h"
#include "storage.h"
#include "alert_feedback.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STOCK_ALERT_TASK_STACK   8192
#define STOCK_ALERT_TASK_PRIO    3
#define STOCK_ALERT_IDLE_WAIT_MS 20000U

static const char *TAG = "stock_alert";

typedef struct {
    scheduler_service_ctx_t *ctx;
    stock_alert_event_t evt;
} stock_alert_task_param_t;

static bool is_threshold_breach(float change_percent, float threshold_pct)
{
    if (threshold_pct > 0.0f) {
        return change_percent >= threshold_pct;
    }
    if (threshold_pct < 0.0f) {
        return change_percent <= threshold_pct;
    }
    return false;
}

static stock_alert_state_t *find_alert_state_by_symbol(scheduler_service_ctx_t *ctx,
                                                        const char *symbol)
{
    if (!ctx || !symbol || symbol[0] == '\0') {
        return NULL;
    }

    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        stock_alert_state_t *state = &ctx->alert_states[i];
        if (!state->active) {
            continue;
        }
        if (strncmp(state->symbol, symbol, sizeof(state->symbol)) == 0) {
            return state;
        }
    }
    return NULL;
}

static void send_alert_failure_telegram(const stock_alert_event_t *evt, const char *reason)
{
    if (!evt || !reason) return;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[StockAlert] %s %s AI failed: %s",
             evt->quote.symbol,
             evt->trigger_up ? "up" : "down",
             reason);

    esp_err_t tg_ret = telegram_bot_send_text(msg);
    if (tg_ret != ESP_OK) {
        ESP_LOGW(TAG, "send fail telegram failed: %s", esp_err_to_name(tg_ret));
    }
}

static esp_err_t load_primary_provider_key(char *api_key, size_t api_key_size)
{
    if (!api_key || api_key_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    api_key[0] = '\0';
    ai_provider_type_t type = ai_provider_get_type();
    storage_ai_load_provider_key((uint8_t)type, api_key, api_key_size);

    if (api_key[0] == '\0' && type == AI_PROVIDER_GEMINI) {
        /* Backward compatible legacy Gemini key slot. */
        storage_ai_load_key(api_key, api_key_size);
    }

    return (api_key[0] != '\0') ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static char *build_ai_prompt(const stock_alert_event_t *evt,
                             const stock_alert_config_t *cfg,
                             const char *global_prompt)
{
    if (!evt || !cfg || cfg->alert_prompt[0] == '\0') {
        return NULL;
    }

    const char *gp = global_prompt ? global_prompt : "";
    // global prompt
    size_t gp_len = strlen(gp);
    size_t prompt_len = strlen(cfg->alert_prompt);
    size_t name_len = strlen(evt->quote.name);
    size_t need = gp_len + prompt_len + name_len + 384U;
    char *prompt = calloc(1, need);
    if (!prompt) {
        return NULL;
    }

    snprintf(prompt, need,
             "%s\n"
             "股票: %s %s\n"
             "現價: %.2f\n"
             "漲跌幅: %.2f%%\n"
             "%s",
             gp,
             evt->quote.symbol,
             evt->quote.name,
             evt->quote.current_price,
             evt->quote.change_percent,
             cfg->alert_prompt);

    return prompt;
}

static char *build_telegram_message(const stock_alert_event_t *evt,
                                    const ai_analysis_result_t *result)
{
    if (!evt || !result || result->analysis[0] == '\0') {
        return NULL;
    }

    const char *direction = evt->trigger_up ? "上漲" : "下跌";
    size_t analysis_len = strlen(result->analysis);
    size_t need = analysis_len + 256U;
    char *msg = calloc(1, need);
    if (!msg) {
        return NULL;
    }

    snprintf(msg, need,
             "[StockAlert] %s %s 觸發%s %.2f%%\n"
             "現價 %.2f / 漲跌幅 %.2f%%\n"
             "%s",
             evt->quote.symbol,
             evt->quote.name,
             direction,
             evt->threshold_pct,
             evt->quote.current_price,
             evt->quote.change_percent,
             result->analysis);

    return msg;
}

static void process_alert_event(const stock_alert_event_t *evt)
{
    if (!evt || evt->quote.symbol[0] == '\0') {
        return;
    }

    stock_alert_config_t cfg = {0};
    esp_err_t load_ret = storage_stock_alert_config_load(evt->quote.symbol, &cfg);
    if (load_ret != ESP_OK || cfg.alert_prompt[0] == '\0' || cfg.threshold_pct == 0.0f) {
        return;
    }

    char api_key[128] = {0};
    if (load_primary_provider_key(api_key, sizeof(api_key)) != ESP_OK) {
        ESP_LOGW(TAG, "skip %s: missing key for provider=%s",
                 evt->quote.symbol, ai_provider_get_name());
        send_alert_failure_telegram(evt, "missing_api_key");
        return;
    }

    bool should_resume_polling = false;
    if (telegram_bot_is_running()) {
        if (!telegram_bot_is_polling_paused()) {
            telegram_bot_pause_polling();
            should_resume_polling = true;
        }
        if (telegram_bot_wait_http_idle(STOCK_ALERT_IDLE_WAIT_MS) != ESP_OK) {
            ESP_LOGW(TAG, "wait telegram idle timeout");
            send_alert_failure_telegram(evt, "wait_http_idle_timeout");
            goto cleanup;
        }
    }

    char global_prompt[513] = {0};
    storage_ai_load_global_prompt(global_prompt, sizeof(global_prompt));

    char *prompt = build_ai_prompt(evt, &cfg, global_prompt);
    if (!prompt) {
        send_alert_failure_telegram(evt, "prompt_build_failed");
        goto cleanup;
    }

    ai_analysis_result_t result = {0};
    esp_err_t ai_ret = ai_provider_analyze_with_prompt_sync(&evt->quote, prompt, api_key, &result);
    free(prompt);
    prompt = NULL;

    if (ai_ret != ESP_OK) {
        ESP_LOGW(TAG, "AI failed symbol=%s provider=%s ret=%s detail=%s",
                 evt->quote.symbol,
                 ai_provider_get_name(),
                 esp_err_to_name(ai_ret),
                 result.analysis[0] ? result.analysis : "(none)");
        send_alert_failure_telegram(evt, "ai_request_failed");
        goto cleanup;
    }

    char *msg = build_telegram_message(evt, &result);
    if (!msg) {
        send_alert_failure_telegram(evt, "telegram_msg_build_failed");
        goto cleanup;
    }

    esp_err_t tg_ret = telegram_bot_send_text(msg);
    free(msg);
    msg = NULL;
    if (tg_ret != ESP_OK) {
        ESP_LOGW(TAG, "send alert telegram failed: %s", esp_err_to_name(tg_ret));
    } else {
        ESP_LOGI(TAG, "alert delivered symbol=%s dir=%s",
                 evt->quote.symbol, evt->trigger_up ? "up" : "down");
    }

cleanup:
    if (should_resume_polling) {
        telegram_bot_resume_polling();
    }
}

static void stock_alert_fire_task(void *arg)
{
    stock_alert_task_param_t *param = (stock_alert_task_param_t *)arg;
    if (!param || !param->ctx) {
        free(param);
        vTaskDelete(NULL);
        return;
    }

    process_alert_event(&param->evt);

    if (param->ctx->alert_mutex &&
        xSemaphoreTake(param->ctx->alert_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        param->ctx->stock_alert_in_flight = false;
        xSemaphoreGive(param->ctx->alert_mutex);
    } else {
        param->ctx->stock_alert_in_flight = false;
    }

    free(param);
    vTaskDelete(NULL);
}

esp_err_t scheduler_service_stock_alert_init(scheduler_service_ctx_t *ctx)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    if (!ctx->alert_mutex) {
        ctx->alert_mutex = xSemaphoreCreateMutex();
        if (!ctx->alert_mutex) {
            ESP_LOGE(TAG, "alert mutex create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    ctx->stock_alert_in_flight = false;
    return scheduler_service_stock_alert_reload_configs(ctx);
}

esp_err_t scheduler_service_stock_alert_reload_configs(scheduler_service_ctx_t *ctx)
{
    if (!ctx || !ctx->alert_mutex) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(ctx->alert_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memset(ctx->alert_states, 0, sizeof(ctx->alert_states));

    for (int i = 0; i < ctx->stock_list.count && i < MAX_STOCK_COUNT; i++) {
        stock_alert_state_t *state = &ctx->alert_states[i];
        stock_alert_config_t cfg = {0};
        const char *symbol = ctx->stock_list.symbols[i];

        state->active = true;
        strlcpy(state->symbol, symbol, sizeof(state->symbol));

        if (storage_stock_alert_config_load(symbol, &cfg) != ESP_OK) {
            state->enabled = false;
            continue;
        }

        state->enabled = (cfg.alert_prompt[0] != '\0' && cfg.threshold_pct != 0.0f);
        state->threshold_pct = cfg.threshold_pct;
        state->latched = false;
    }

    xSemaphoreGive(ctx->alert_mutex);
    ESP_LOGI(TAG, "alert config reloaded count=%u", (unsigned)ctx->stock_list.count);
    return ESP_OK;
}

void scheduler_service_stock_alert_on_quote(scheduler_service_ctx_t *ctx, const stock_quote_t *quote)
{
    if (!ctx || !quote || quote->symbol[0] == '\0') return;
    if (!ctx->alert_mutex) return;
    if (!quote->is_valid || quote->is_market_closed) return;

    if (xSemaphoreTake(ctx->alert_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }

    stock_alert_state_t *state = find_alert_state_by_symbol(ctx, quote->symbol);
    if (!state || !state->enabled) {
        xSemaphoreGive(ctx->alert_mutex);
        return;
    }

    bool breach = is_threshold_breach(quote->change_percent, state->threshold_pct);
    bool trigger = breach && !state->latched;
    bool trigger_up = (state->threshold_pct > 0.0f);
    state->latched = breach;

    if (!trigger) {
        xSemaphoreGive(ctx->alert_mutex);
        return;
    }

    if (ctx->stock_alert_in_flight) {
        ESP_LOGW(TAG, "alert busy, drop symbol=%s dir=%s",
                 quote->symbol, trigger_up ? "up" : "down");
        xSemaphoreGive(ctx->alert_mutex);
        return;
    }

    alert_feedback_play(trigger_up ? ALERT_FEEDBACK_UP : ALERT_FEEDBACK_DOWN);

    stock_alert_task_param_t *param = calloc(1, sizeof(*param));
    if (!param) {
        ctx->stock_alert_in_flight = false;
        state->latched = false;
        xSemaphoreGive(ctx->alert_mutex);
        return;
    }

    param->ctx = ctx;
    param->evt.quote = *quote;
    param->evt.trigger_up = trigger_up;
    param->evt.threshold_pct = fabsf(state->threshold_pct);

    ctx->stock_alert_in_flight = true;
    xSemaphoreGive(ctx->alert_mutex);

    BaseType_t ok = xTaskCreatePinnedToCore(stock_alert_fire_task,
                                            "stock_alert_once",
                                            STOCK_ALERT_TASK_STACK,
                                            param,
                                            STOCK_ALERT_TASK_PRIO,
                                            NULL,
                                            1);
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "create stock alert task failed");
        free(param);
        if (ctx->alert_mutex &&
            xSemaphoreTake(ctx->alert_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            ctx->stock_alert_in_flight = false;
            state->latched = false;
            xSemaphoreGive(ctx->alert_mutex);
        } else {
            ctx->stock_alert_in_flight = false;
        }
        return;
    }

    ESP_LOGI(TAG, "alert triggered symbol=%s dir=%s cp=%.2f th=%.2f",
             quote->symbol,
             trigger_up ? "up" : "down",
             quote->change_percent,
             state->threshold_pct);
}
