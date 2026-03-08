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

static size_t utf8_codepoint_len(const char *s)
{
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) return 1;
    if ((c0 & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[1];
        return ((c1 & 0xC0) == 0x80) ? 2 : 0;
    }
    if ((c0 & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        return (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) ? 3 : 0;
    }
    if ((c0 & 0xF8) == 0xF0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        unsigned char c3 = (unsigned char)s[3];
        return (((c1 & 0xC0) == 0x80) &&
                ((c2 & 0xC0) == 0x80) &&
                ((c3 & 0xC0) == 0x80)) ? 4 : 0;
    }
    return 0;
}

static size_t utf8_prefix_limit(const char *s, size_t max_bytes, size_t max_chars)
{
    size_t pos = 0;
    size_t chars = 0;
    while (s[pos] != '\0' && chars < max_chars && pos < max_bytes) {
        size_t cp_len = utf8_codepoint_len(s + pos);
        if (cp_len == 0 || (pos + cp_len) > max_bytes) {
            break;
        }
        pos += cp_len;
        chars++;
    }
    return pos;
}

static void trim_analysis_inplace(ai_analysis_result_t *result)
{
    if (!result) return;
    if (AI_ANALYSIS_MAX_LEN == 0) return;

    size_t hard_max_bytes = AI_ANALYSIS_MAX_LEN - 1;
    size_t soft_max_bytes = AI_ANALYSIS_MAX_BYTES;
    if (soft_max_bytes > hard_max_bytes) {
        soft_max_bytes = hard_max_bytes;
    }

    size_t raw_len = strlen(result->analysis);
    size_t keep_len = utf8_prefix_limit(result->analysis, soft_max_bytes, AI_ANALYSIS_MAX_CHARS);
    if (keep_len < raw_len) {
        result->analysis[keep_len] = '\0';
        ESP_LOGW(TAG, "analysis trimmed raw=%u keep=%u chars<=%u bytes<=%u",
                 (unsigned)raw_len,
                 (unsigned)keep_len,
                 (unsigned)AI_ANALYSIS_MAX_CHARS,
                 (unsigned)soft_max_bytes);
    }
}

static stock_context_t quote_to_context(const stock_quote_t *q)
{
    stock_context_t ctx;
    strlcpy(ctx.symbol, q->symbol, sizeof(ctx.symbol));
    strlcpy(ctx.name, q->name, sizeof(ctx.name));
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
            strlcpy(out_key, candidate, out_size);
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
    strlcpy(active_key, args->api_key, sizeof(active_key));

    esp_err_t ret = s_providers[args->provider_type]->analyze(
                        &ctx, prompt, active_key, &result);
    trim_analysis_inplace(&result);
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
    if (api_key) {
        strlcpy(args->api_key, api_key, sizeof(args->api_key));
    } else {
        args->api_key[0] = '\0';
    }
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
        strlcpy(active_key, api_key, sizeof(active_key));
    } else {
        ai_provider_get_active_api_key(active_key, sizeof(active_key));
    }

    memset(result, 0, sizeof(*result));
    esp_err_t ret = s_providers[s_current_type]->analyze(
                        &ctx, prompt, active_key, result);
    trim_analysis_inplace(result);
    result->error_code = ret;
    return ret;
}

esp_err_t ai_provider_analyze_prompt_sync(const char *prompt,
                                           const char *api_key,
                                           ai_analysis_result_t *result)
{
    if (!prompt || !result) return ESP_ERR_INVALID_ARG;

    stock_context_t empty_ctx = {0};
    char active_key[128] = {0};
    if (api_key && api_key[0] != '\0') {
        strlcpy(active_key, api_key, sizeof(active_key));
    } else {
        ai_provider_get_active_api_key(active_key, sizeof(active_key));
    }

    if (active_key[0] == '\0') {
        strlcpy(result->analysis, "No API key configured", AI_ANALYSIS_MAX_LEN);
        result->error_code = ESP_ERR_NOT_FOUND;
        return ESP_ERR_NOT_FOUND;
    }

    memset(result, 0, sizeof(*result));
    esp_err_t ret = s_providers[s_current_type]->analyze(
                        &empty_ctx, prompt, active_key, result);
    trim_analysis_inplace(result);
    result->error_code = ret;
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "prompt analyze failed provider=%s ret=%s detail=%s",
                 s_providers[s_current_type]->name,
                 esp_err_to_name(ret),
                 result->analysis[0] ? result->analysis : "(none)");
    }
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
