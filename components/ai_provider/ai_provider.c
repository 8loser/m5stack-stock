#include "ai_provider.h"
#include "app_config.h"
#include "storage.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

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

/* 遠端 Token 設定（session 級別，不持久化）*/
static char s_session_api_key[128]     = {0};  /* 遠端抓取的 key，優先使用 */
static bool s_has_remote_token         = false;

/* 遠端 Token HTTP 事件處理（靜態函數，供 fetch_remote_token_config 使用）*/
typedef struct { char *b; size_t sz; size_t len; } token_ctx_t;

static esp_err_t _token_cfg_http_ev(esp_http_client_event_t *e)
{
    token_ctx_t *c = e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && c->len + e->data_len < c->sz) {
        memcpy(c->b + c->len, e->data, e->data_len);
        c->len += e->data_len;
    }
    return ESP_OK;
}

/* 預設 Prompt 模板 */
static const char *DEFAULT_PROMPT_TEMPLATE =
    "你是台灣股市分析師。請分析以下股票資訊，給出買入/賣出/觀望建議：\n"
    "股票：%s (%s)\n"
    "現價：%.2f 元，漲跌：%.2f%%\n"
    "今日區間：%.2f - %.2f 元，成交量：%ld 張\n"
    "%s\n"   /* 遠端 system_prompt 附加 */
    "\n請以 JSON 格式回應：\n"
    "{\"signal\": \"buy|sell|hold\", \"confidence\": 0-100, "
    "\"analysis\": \"說明（繁體中文，100字以內）\"}";

void ai_provider_build_prompt(const stock_context_t *ctx,
                               const char *prompt_template,
                               char *out, size_t out_size)
{
    const remote_prompt_config_t *rp = ai_provider_get_remote_prompt();

    /* 優先使用遠端自訂模板；否則用預設 */
    const char *tmpl = (rp->is_loaded && strlen(rp->analysis_template) > 0)
                       ? rp->analysis_template
                       : DEFAULT_PROMPT_TEMPLATE;

    const char *extra = (rp->is_loaded && strlen(rp->system_prompt) > 0)
                        ? rp->system_prompt : "";

    snprintf(out, out_size, tmpl,
             ctx->name, ctx->symbol,
             ctx->current_price, ctx->change_percent,
             ctx->low_price, ctx->high_price,
             ctx->volume,
             extra);
}

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

esp_err_t ai_provider_fetch_remote_token_config(const char *url)
{
    if (!url || strlen(url) == 0) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "抓取遠端 Token 設定: %s", url);

    char buf[1024] = {0};
    token_ctx_t ctx = {.b = buf, .sz = sizeof(buf) - 1, .len = 0};

    esp_http_client_config_t cfg = {
        .url               = url,
        .event_handler     = _token_cfg_http_ev,
        .user_data         = &ctx,
        .timeout_ms        = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = false, /* 允許自動跟隨 30x */
        .max_redirection_count = 3,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "遠端 Token 設定抓取失敗 HTTP %d", status);
        return ESP_FAIL;
    }

    buf[ctx.len] = '\0';
    ESP_LOGD(TAG, "遠端 Token Config: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    /* 解析 provider */
    cJSON *prov = cJSON_GetObjectItem(root, "provider");
    if (prov && cJSON_IsString(prov)) {
        const char *pname = prov->valuestring;
        if (strcasecmp(pname, "claude") == 0)
            s_current_type = AI_PROVIDER_CLAUDE;
        else if (strcasecmp(pname, "openai") == 0)
            s_current_type = AI_PROVIDER_OPENAI;
        else
            s_current_type = AI_PROVIDER_GEMINI;
        ESP_LOGI(TAG, "遠端設定 Provider: %s", s_providers[s_current_type]->name);
    }

    /* 解析 api_key */
    cJSON *key = cJSON_GetObjectItem(root, "api_key");
    if (key && cJSON_IsString(key) && strlen(key->valuestring) > 8) {
        strncpy(s_session_api_key, key->valuestring, sizeof(s_session_api_key) - 1);
        s_has_remote_token = true;
        ESP_LOGI(TAG, "遠端 API Key 已載入（長度=%d）", (int)strlen(s_session_api_key));
    }

    cJSON_Delete(root);
    return ESP_OK;
}

void ai_provider_get_active_api_key(char *out, size_t out_size)
{
    if (s_has_remote_token && strlen(s_session_api_key) > 0) {
        /* 優先使用遠端抓取的 key */
        strncpy(out, s_session_api_key, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        /* 退回本地 NVS 儲存的 key */
        storage_ai_load_key(out, out_size);
    }
}

esp_err_t ai_provider_init(QueueHandle_t result_queue)
{
    s_result_queue = result_queue;

    uint8_t saved_type = 0;
    storage_ai_load_provider(&saved_type);
    s_current_type = (ai_provider_type_t)saved_type;

    /* 步驟1：嘗試從遠端抓取 Token 設定（開機自動執行）*/
    char remote_cfg_url[256] = {0};
    storage_ai_load_remote_cfg_url(remote_cfg_url, sizeof(remote_cfg_url));
    if (strlen(remote_cfg_url) > 0) {
        ESP_LOGI(TAG, "開機抓取遠端 Token 設定: %s", remote_cfg_url);
        esp_err_t ret = ai_provider_fetch_remote_token_config(remote_cfg_url);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "遠端 Token 設定載入成功，Provider=%s",
                     s_providers[s_current_type]->name);
        } else {
            ESP_LOGW(TAG, "遠端 Token 設定失敗，使用本地設定");
        }
    }

    /* 步驟2：嘗試下載遠端 Prompt */
    char prompt_url[256] = {0};
    storage_ai_load_prompt_url(prompt_url, sizeof(prompt_url));
    if (strlen(prompt_url) > 0) {
        ESP_LOGI(TAG, "嘗試載入遠端 Prompt: %s", prompt_url);
        ai_provider_fetch_remote_prompt(prompt_url);
    }

    ESP_LOGI(TAG, "AI Provider 初始化完成，使用 %s（遠端Token=%s）",
             s_providers[s_current_type]->name,
             s_has_remote_token ? "是" : "否");
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
} analyze_task_args_t;

static void analyze_task(void *arg)
{
    analyze_task_args_t *args = (analyze_task_args_t *)arg;
    ai_analysis_result_t result = {0};

    stock_context_t ctx = quote_to_context(&args->quote);

    char prompt[1536];
    ai_provider_build_prompt(&ctx, NULL, prompt, sizeof(prompt));

    /* 優先使用遠端 Token，次選傳入的 key */
    char active_key[128] = {0};
    if (s_has_remote_token && strlen(s_session_api_key) > 0) {
        strncpy(active_key, s_session_api_key, sizeof(active_key) - 1);
    } else {
        strncpy(active_key, args->api_key, sizeof(active_key) - 1);
    }

    esp_err_t ret = s_providers[s_current_type]->analyze(
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
    stock_context_t ctx = quote_to_context(quote);

    char prompt[1536];
    ai_provider_build_prompt(&ctx, NULL, prompt, sizeof(prompt));

    memset(result, 0, sizeof(*result));
    esp_err_t ret = s_providers[s_current_type]->analyze(
                        &ctx, prompt, api_key, result);
    result->error_code = ret;
    return ret;
}
