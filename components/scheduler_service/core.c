#include "internal.h"
#include "sleep_manager.h"
#include "app_config.h"
#include "twse_client.h"
#include "ai_provider.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

static const char *TAG = "scheduler";

static scheduler_service_ctx_t s_ctx = {0};

scheduler_service_ctx_t *scheduler_service_ctx(void)
{
    return &s_ctx;
}

static void quote_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    scheduler_service_ctx_t *ctx = scheduler_service_ctx();
    if (ctx->scheduler_task) {
        xTaskNotify(ctx->scheduler_task, SCHEDULER_SERVICE_NOTIFY_QUOTE_BIT, eSetBits);
    }
}

static esp_err_t test_key_with_retry(ai_provider_type_t type, const char *key, int *status)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_err_t err = ai_provider_test_key(type, key, status);
        if (err == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "AI test key attempt %d/%d failed for provider %d, retrying...",
                 attempt + 1, 3, (int)type);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return ESP_FAIL;
}

static void execute_cmd_test_ai_key(const sched_cmd_item_t *cmd)
{
    sched_ai_test_result_t *r = (sched_ai_test_result_t *)cmd->result;
    int sel = cmd->selected_provider;
    bool has_key = (cmd->api_key[0] != '\0');
    char key_buf[128] = {0};

    /* --- Gemini --- */
    if (sel < 0 || sel == AI_PROVIDER_GEMINI) {
        if (sel == AI_PROVIDER_GEMINI && has_key) {
            strlcpy(key_buf, cmd->api_key, sizeof(key_buf));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_GEMINI, key_buf, sizeof(key_buf));
            if (key_buf[0] == '\0') {
                storage_ai_load_key(key_buf, sizeof(key_buf));
            }
        }
        r->gemini.configured = (key_buf[0] != '\0');
        if (r->gemini.configured) {
            r->gemini.err = test_key_with_retry(AI_PROVIDER_GEMINI, key_buf, &r->gemini.status_code);
            r->gemini.ok = (r->gemini.err == ESP_OK && r->gemini.status_code == 200);
            if (r->gemini.ok) r->passed++; else r->failed++;
        } else {
            r->skipped++;
        }
        memset(key_buf, 0, sizeof(key_buf));
    }

    /* --- Claude --- */
    if (sel < 0 || sel == AI_PROVIDER_CLAUDE) {
        if (sel == AI_PROVIDER_CLAUDE && has_key) {
            strlcpy(key_buf, cmd->api_key, sizeof(key_buf));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_CLAUDE, key_buf, sizeof(key_buf));
        }
        r->claude.configured = (key_buf[0] != '\0');
        if (r->claude.configured) {
            r->claude.err = test_key_with_retry(AI_PROVIDER_CLAUDE, key_buf, &r->claude.status_code);
            r->claude.ok = (r->claude.err == ESP_OK && r->claude.status_code == 200);
            if (r->claude.ok) r->passed++; else r->failed++;
        } else {
            r->skipped++;
        }
        memset(key_buf, 0, sizeof(key_buf));
    }

    /* --- OpenAI --- */
    if (sel < 0 || sel == AI_PROVIDER_OPENAI) {
        if (sel == AI_PROVIDER_OPENAI && has_key) {
            strlcpy(key_buf, cmd->api_key, sizeof(key_buf));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_OPENAI, key_buf, sizeof(key_buf));
        }
        r->openai.configured = (key_buf[0] != '\0');
        if (r->openai.configured) {
            r->openai.err = test_key_with_retry(AI_PROVIDER_OPENAI, key_buf, &r->openai.status_code);
            r->openai.ok = (r->openai.err == ESP_OK && r->openai.status_code == 200);
            if (r->openai.ok) r->passed++; else r->failed++;
        } else {
            r->skipped++;
        }
    }
}

void scheduler_service_process_cmd_queue(scheduler_service_ctx_t *ctx)
{
    sched_cmd_item_t cmd;
    while (xQueueReceive(ctx->cmd_queue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
        case SCHED_CMD_TEST_AI_KEY:
            execute_cmd_test_ai_key(&cmd);
            break;
        }
        if (cmd.done) {
            xSemaphoreGive(cmd.done);
        }
    }
}

static void scheduler_service_task(void *arg)
{
    scheduler_service_ctx_t *ctx = (scheduler_service_ctx_t *)arg;
    uint32_t last_diag_ms = 0;

    while (!scheduler_service_is_wifi_connected()) {
        scheduler_service_publish_scheduler_tick(ctx, false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    scheduler_service_init_sntp(ctx);

    scheduler_service_do_fetch_quotes(ctx, false);

    while (1) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(1000));
        scheduler_service_publish_scheduler_tick(
            ctx, (bits & SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT) != 0);

        if (bits & SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT) {
            if (!ctx->quote_polling_paused) {
                scheduler_service_do_fetch_quotes(ctx, true);
            }
        } else if (bits & SCHEDULER_SERVICE_NOTIFY_QUOTE_BIT) {
            if (!ctx->quote_polling_paused) {
                scheduler_service_do_fetch_quotes(ctx, false);
            }
        }

        if (bits & SCHEDULER_SERVICE_NOTIFY_CMD_BIT) {
            scheduler_service_process_cmd_queue(ctx);
        }

        scheduler_service_check_at_time(ctx);

        if (sleep_manager_is_enabled() && sleep_manager_should_sleep()) {
            ESP_LOGI(TAG, "進入休市睡眠模式");
            scheduler_service_stop();
            sleep_manager_enter(MARKET_OPEN_HOUR, MARKET_OPEN_MIN > 5
                                ? MARKET_OPEN_MIN - 5 : 0);
        }

        uint32_t now_ms = (uint32_t)esp_log_timestamp();
        if (now_ms - last_diag_ms >= 30000U) {
            UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "diag stack_hwm scheduler=%u", (unsigned)wm);
            last_diag_ms = now_ms;
        }
    }
}

bool scheduler_service_is_quote_fetch_in_flight(void)
{
    return s_ctx.quote_fetch_in_flight;
}

esp_err_t scheduler_service_wait_quote_fetch_idle(uint32_t timeout_ms)
{
    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ms * 1000LL;

    while (s_ctx.quote_fetch_in_flight) {
        if ((esp_timer_get_time() - start_us) >= timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

esp_err_t scheduler_service_init(QueueHandle_t quote_queue)
{
    s_ctx = (scheduler_service_ctx_t){0};
    s_ctx.quote_queue = quote_queue;
    s_ctx.cmd_queue = xQueueCreate(1, sizeof(sched_cmd_item_t));
    s_ctx.quote_polling_paused = false;

    storage_schedule_load(&s_ctx.config);
    storage_stocks_load(&s_ctx.stock_list);
    if (scheduler_service_stock_alert_init(&s_ctx) != ESP_OK) {
        ESP_LOGE(TAG, "Stock alert init failed");
        return ESP_FAIL;
    }
    s_ctx.at_time_entries = heap_caps_calloc(MAX_AT_TIME_COUNT, sizeof(at_time_entry_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_ctx.at_time_entries) {
        storage_at_time_load_all(s_ctx.at_time_entries, &s_ctx.at_time_count);
    }
    s_ctx.at_time_prev_minute = -1;
    s_ctx.at_time_fired_bitmask = 0;

    sleep_manager_init();

    s_ctx.quote_timer = xTimerCreate("quote_tmr",
                                 pdMS_TO_TICKS(s_ctx.config.quote_interval_s * 1000),
                                 pdTRUE,
                                 NULL,
                                 quote_timer_cb);

    if (!s_ctx.quote_timer) {
        ESP_LOGE(TAG, "Timer 建立失敗");
        return ESP_FAIL;
    }

    xTimerStart(s_ctx.quote_timer, 0);

    BaseType_t res = xTaskCreatePinnedToCore(scheduler_service_task, "scheduler",
                                              STACK_SCHEDULER, &s_ctx,
                                              TASK_PRIO_SCHEDULER,
                                              &s_ctx.scheduler_task, 1);

    ESP_LOGI(TAG, "Scheduler 啟動：報價間隔=%ds",
             s_ctx.config.quote_interval_s);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

esp_err_t scheduler_service_apply_config(const schedule_config_t *cfg)
{
    s_ctx.config = *cfg;
    storage_schedule_save(cfg);

    if (s_ctx.quote_timer) {
        xTimerChangePeriod(s_ctx.quote_timer,
                           pdMS_TO_TICKS(cfg->quote_interval_s * 1000), 0);
    }
    ESP_LOGI(TAG, "排程設定已更新：報價=%ds market_only=%d",
             cfg->quote_interval_s, cfg->market_only ? 1 : 0);
    return ESP_OK;
}

void scheduler_service_get_config(schedule_config_t *cfg)
{
    *cfg = s_ctx.config;
}

void scheduler_service_trigger_quote_now(void)
{
    if (s_ctx.quote_polling_paused) {
        ESP_LOGW(TAG, "trigger_quote_now ignored: polling paused (portal active)");
        return;
    }
    if (s_ctx.scheduler_task) {
        xTaskNotify(s_ctx.scheduler_task, SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT, eSetBits);
    }
}

uint32_t scheduler_service_get_seconds_to_next_quote(void)
{
    if (!s_ctx.quote_timer) {
        return 0;
    }

    TickType_t expiry_tick = xTimerGetExpiryTime(s_ctx.quote_timer);
    TickType_t now_tick = xTaskGetTickCount();
    TickType_t diff_tick = (expiry_tick > now_tick) ? (expiry_tick - now_tick) : 0;

    return (uint32_t)(diff_tick / configTICK_RATE_HZ);
}

esp_err_t scheduler_service_reload_stock_list(void)
{
    esp_err_t ret = storage_stocks_load(&s_ctx.stock_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重載股票清單失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = scheduler_service_stock_alert_reload_configs(&s_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重載 stock alert 設定失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "股票清單已重載，count=%u（下個排程週期生效）", s_ctx.stock_list.count);
    return ESP_OK;
}

esp_err_t scheduler_service_reload_at_time(void)
{
    if (!s_ctx.at_time_entries) {
        s_ctx.at_time_entries = heap_caps_calloc(MAX_AT_TIME_COUNT, sizeof(at_time_entry_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_ctx.at_time_entries) {
            ESP_LOGE(TAG, "AtTime reload OOM");
            return ESP_ERR_NO_MEM;
        }
    }
    esp_err_t ret = storage_at_time_load_all(s_ctx.at_time_entries, &s_ctx.at_time_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AtTime reload failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_ctx.at_time_fired_bitmask = 0;
    ESP_LOGI(TAG, "AtTime entries reloaded, count=%u", (unsigned)s_ctx.at_time_count);
    return ESP_OK;
}

void scheduler_service_stop(void)
{
    if (s_ctx.quote_timer) {
        xTimerStop(s_ctx.quote_timer, 0);
    }
    s_ctx.quote_polling_paused = true;
    twse_client_stop_task();
    ESP_LOGI(TAG, "所有排程已停止");
}

esp_err_t scheduler_service_pause_quote_polling(void)
{
    if (!s_ctx.quote_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ctx.quote_polling_paused) {
        return ESP_OK;
    }
    if (xTimerStop(s_ctx.quote_timer, 0) != pdPASS) {
        ESP_LOGW(TAG, "暫停週期報價抓取失敗");
        return ESP_FAIL;
    }
    s_ctx.quote_polling_paused = true;
    ESP_LOGI(TAG, "已暫停週期報價抓取");
    return ESP_OK;
}

esp_err_t scheduler_service_resume_quote_polling(void)
{
    if (!s_ctx.quote_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ctx.quote_polling_paused) {
        return ESP_OK;
    }
    if (xTimerStart(s_ctx.quote_timer, 0) != pdPASS) {
        ESP_LOGW(TAG, "恢復週期報價抓取失敗");
        return ESP_FAIL;
    }
    s_ctx.quote_polling_paused = false;
    ESP_LOGI(TAG, "已恢復週期報價抓取");

    /* Trigger immediate fetch so dashboard shows fresh data right away */
    if (s_ctx.scheduler_task) {
        xTaskNotify(s_ctx.scheduler_task, SCHEDULER_SERVICE_NOTIFY_FORCE_QUOTE_BIT, eSetBits);
    }
    return ESP_OK;
}

esp_err_t scheduler_service_cmd_test_ai_key(int selected_provider,
                                             const char *provided_api_key,
                                             sched_ai_test_result_t *result,
                                             uint32_t timeout_ms)
{
    if (!s_ctx.cmd_queue || !s_ctx.scheduler_task) {
        return ESP_ERR_INVALID_STATE;
    }

    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done) {
        return ESP_ERR_NO_MEM;
    }

    memset(result, 0, sizeof(*result));

    sched_cmd_item_t cmd = {
        .type = SCHED_CMD_TEST_AI_KEY,
        .result = result,
        .done = done,
        .selected_provider = selected_provider,
    };
    if (provided_api_key && provided_api_key[0] != '\0') {
        strlcpy(cmd.api_key, provided_api_key, sizeof(cmd.api_key));
    }

    if (xQueueSend(s_ctx.cmd_queue, &cmd, 0) != pdTRUE) {
        vSemaphoreDelete(done);
        return ESP_FAIL;
    }

    xTaskNotify(s_ctx.scheduler_task, SCHEDULER_SERVICE_NOTIFY_CMD_BIT, eSetBits);

    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(done, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ret = ESP_ERR_TIMEOUT;
    }

    vSemaphoreDelete(done);
    return ret;
}

void scheduler_service_process_alert_quote(const stock_quote_t *quote)
{
    scheduler_service_stock_alert_on_quote(&s_ctx, quote);
}

esp_err_t scheduler_service_reload_stock_alert_configs(void)
{
    return scheduler_service_stock_alert_reload_configs(&s_ctx);
}
