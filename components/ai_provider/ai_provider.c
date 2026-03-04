#include "ai_provider.h"
#include "app_config.h"
#include "storage.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ai_provider";

/* 前向宣告各 Provider */
extern const ai_provider_ops_t gemini_ops;
extern const ai_provider_ops_t claude_ops;
extern const ai_provider_ops_t openai_ops;

static const ai_provider_ops_t *s_providers[] = {
    &gemini_ops,
    &claude_ops,
    &openai_ops,
};

static ai_provider_type_t s_current_type = AI_PROVIDER_GEMINI;
static QueueHandle_t      s_result_queue = NULL;

static stock_context_t quote_to_context(const stock_quote_t *q)
{
    stock_context_t ctx;
    strncpy(ctx.symbol, q->symbol, 7);
    strncpy(ctx.name,   q->name,   31);
    ctx.current_price  = q->current_price;
    ctx.change_percent = q->change_percent;
    ctx.low_price      = q->low_price;
    ctx.high_price     = q->high_price;
    ctx.volume         = q->volume;
    return ctx;
}

static void load_provider_key(ai_provider_type_t type, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    storage_ai_load_provider_key((uint8_t)type, out, out_size);
    if (out[0] == '\0' && type == AI_PROVIDER_GEMINI) {
        /* 與舊版單一 key 相容（預設視為 Gemini）*/
        storage_ai_load_key(out, out_size);
    }
}

static void select_provider_and_key(char *out_key, size_t out_size)
{
    if (!out_key || out_size == 0) return;
    out_key[0] = '\0';

    load_provider_key(s_current_type, out_key, out_size);
    if (out_key[0] != '\0') return;

    for (int i = 0; i < 3; i++) {
        char candidate[128] = {0};
        load_provider_key((ai_provider_type_t)i, candidate, sizeof(candidate));
        if (candidate[0] != '\0') {
            s_current_type = (ai_provider_type_t)i;
            storage_ai_save_provider((uint8_t)s_current_type);
            strncpy(out_key, candidate, out_size - 1);
            out_key[out_size - 1] = '\0';
            ESP_LOGI(TAG, "自動切換 AI Provider: %s", s_providers[s_current_type]->name);
            return;
        }
    }
}

void ai_provider_get_active_api_key(char *out, size_t out_size)
{
    select_provider_and_key(out, out_size);
}

esp_err_t ai_provider_reload_local_config(void)
{
    return ESP_OK;
}

esp_err_t ai_provider_init(QueueHandle_t result_queue)
{
    s_result_queue = result_queue;

    uint8_t saved_type = 0;
    storage_ai_load_provider(&saved_type);
    s_current_type = (ai_provider_type_t)saved_type;

    ESP_LOGI(TAG, "AI Provider 初始化完成，使用 %s（本機設定模式）",
             s_providers[s_current_type]->name);
    return ESP_OK;
}

esp_err_t ai_provider_set_type(ai_provider_type_t type)
{
    if (type >= 3) return ESP_ERR_INVALID_ARG;
    s_current_type = type;
    storage_ai_save_provider((uint8_t)type);
    ESP_LOGI(TAG, "切換 AI Provider: %s", s_providers[type]->name);
    return ESP_OK;
}

ai_provider_type_t ai_provider_get_type(void)
{
    return s_current_type;
}

const char *ai_provider_get_name(void)
{
    return s_providers[s_current_type]->name;
}

/* 非同步任務參數 */
typedef struct {
    stock_quote_t quote;
    char          api_key[128];
    ai_provider_type_t provider_type;
} analyze_task_args_t;

static void analyze_task(void *arg)
{
    analyze_task_args_t *args = (analyze_task_args_t *)arg;
    ai_analysis_result_t result = {0};
    const char *prompt = "";

    stock_context_t ctx = quote_to_context(&args->quote);

    char active_key[128] = {0};
    strncpy(active_key, args->api_key, sizeof(active_key) - 1);

    esp_err_t ret = s_providers[args->provider_type]->analyze(
                        &ctx, prompt, active_key, &result);
    result.error_code = ret;

    if (s_result_queue) {
        xQueueOverwrite(s_result_queue, &result);
    }

    free(args);
    vTaskDelete(NULL);
}

esp_err_t ai_provider_analyze_async(const stock_quote_t *quote,
                                     const char *api_key)
{
    analyze_task_args_t *args = malloc(sizeof(analyze_task_args_t));
    if (!args) return ESP_ERR_NO_MEM;

    args->quote = *quote;
    strncpy(args->api_key, api_key, 127);
    args->provider_type = s_current_type;

    BaseType_t res = xTaskCreatePinnedToCore(analyze_task, "ai_analyze",
                                              STACK_AI, args,
                                              TASK_PRIO_AI,
                                              NULL, 0);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

esp_err_t ai_provider_analyze_sync(const stock_quote_t *quote,
                                    const char *api_key,
                                    ai_analysis_result_t *result)
{
    const char *prompt = "";
    stock_context_t ctx = quote_to_context(quote);

    char active_key[128] = {0};
    if (api_key && strlen(api_key) > 0) {
        strncpy(active_key, api_key, sizeof(active_key) - 1);
    } else {
        ai_provider_get_active_api_key(active_key, sizeof(active_key));
    }

    memset(result, 0, sizeof(*result));
    esp_err_t ret = s_providers[s_current_type]->analyze(
                        &ctx, prompt, active_key, result);
    result->error_code = ret;
    return ret;
}

esp_err_t ai_provider_test_key(ai_provider_type_t type,
                               const char *api_key,
                               int *out_status_code)
{
    if (out_status_code) {
        *out_status_code = 0;
    }
    if (type < AI_PROVIDER_GEMINI || type > AI_PROVIDER_OPENAI) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!api_key || api_key[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    const ai_provider_ops_t *ops = s_providers[type];
    if (!ops || !ops->test_key) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ops->test_key(api_key, out_status_code);
}
