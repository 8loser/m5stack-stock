#include "ai_provider.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ai_gemini";

#define GEMINI_URL_FMT \
    "https://generativelanguage.googleapis.com/v1beta/models/" \
    "gemini-1.5-flash:generateContent?key=%s"
#define RESP_BUF_SIZE   4096

typedef struct { char *buf; size_t size; size_t len; } resp_ctx_t;

static esp_err_t resp_handler(esp_http_client_event_t *evt)
{
    resp_ctx_t *ctx = evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->len + evt->data_len < ctx->size) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
        }
    }
    return ESP_OK;
}

static ai_signal_t parse_signal(const char *s)
{
    if (!s) return AI_SIGNAL_UNKNOWN;
    if (strncasecmp(s, "buy",  3) == 0) return AI_SIGNAL_BUY;
    if (strncasecmp(s, "sell", 4) == 0) return AI_SIGNAL_SELL;
    if (strncasecmp(s, "hold", 4) == 0) return AI_SIGNAL_HOLD;
    return AI_SIGNAL_UNKNOWN;
}

static esp_err_t gemini_analyze(const stock_context_t *ctx,
                                 const char *prompt,
                                 const char *api_key,
                                 ai_analysis_result_t *result)
{
    char url[256];
    snprintf(url, sizeof(url), GEMINI_URL_FMT, api_key);

    /* 建構請求 JSON */
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content = cJSON_CreateObject();
    cJSON *parts = cJSON_AddArrayToObject(content, "parts");
    cJSON *part = cJSON_CreateObject();
    cJSON_AddStringToObject(part, "text", prompt);
    cJSON_AddItemToArray(parts, part);
    cJSON_AddItemToArray(contents, content);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    char *resp_buf = malloc(RESP_BUF_SIZE);
    if (!resp_buf) { free(body); return ESP_ERR_NO_MEM; }

    resp_ctx_t resp_ctx = {.buf = resp_buf, .size = RESP_BUF_SIZE - 1, .len = 0};

    esp_http_client_config_t cfg = {
        .url              = url,
        .method           = HTTP_METHOD_POST,
        .event_handler    = resp_handler,
        .user_data        = &resp_ctx,
        .timeout_ms       = AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "Gemini HTTP %d: %s", status, esp_err_to_name(ret));
        free(resp_buf);
        strncpy(result->analysis, "Gemini API 請求失敗",
                AI_ANALYSIS_MAX_LEN - 1);
        result->signal = AI_SIGNAL_UNKNOWN;
        return ESP_FAIL;
    }

    resp_buf[resp_ctx.len] = '\0';

    /* 解析 Gemini 回應：candidates[0].content.parts[0].text */
    cJSON *resp = cJSON_Parse(resp_buf);
    free(resp_buf);

    if (!resp) { return ESP_FAIL; }

    const char *text = NULL;
    cJSON *candidates = cJSON_GetObjectItem(resp, "candidates");
    if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
        cJSON *c0 = cJSON_GetArrayItem(candidates, 0);
        cJSON *c_parts = cJSON_GetObjectItemCaseSensitive(
                            cJSON_GetObjectItem(c0, "content"), "parts");
        if (cJSON_IsArray(c_parts)) {
            cJSON *p0 = cJSON_GetArrayItem(c_parts, 0);
            cJSON *t = cJSON_GetObjectItem(p0, "text");
            if (t && cJSON_IsString(t)) text = t->valuestring;
        }
    }

    if (!text) {
        cJSON_Delete(resp);
        return ESP_FAIL;
    }

    /* 從 text 中找 JSON 物件 */
    const char *json_start = strchr(text, '{');
    const char *json_end   = strrchr(text, '}');
    if (json_start && json_end && json_end > json_start) {
        char json_str[512];
        size_t json_len = json_end - json_start + 1;
        if (json_len < sizeof(json_str)) {
            memcpy(json_str, json_start, json_len);
            json_str[json_len] = '\0';

            cJSON *ai_json = cJSON_Parse(json_str);
            if (ai_json) {
                cJSON *sig = cJSON_GetObjectItem(ai_json, "signal");
                cJSON *conf = cJSON_GetObjectItem(ai_json, "confidence");
                cJSON *anal = cJSON_GetObjectItem(ai_json, "analysis");

                result->signal     = parse_signal(sig ? sig->valuestring : NULL);
                result->confidence = conf ? conf->valueint : 50;
                if (anal && cJSON_IsString(anal)) {
                    strncpy(result->analysis, anal->valuestring,
                            AI_ANALYSIS_MAX_LEN - 1);
                }
                cJSON_Delete(ai_json);
            }
        }
    }

    cJSON_Delete(resp);
    ESP_LOGI(TAG, "Gemini 分析完成: signal=%d confidence=%d",
             result->signal, result->confidence);
    return ESP_OK;
}

const ai_provider_ops_t gemini_ops = {
    .analyze = gemini_analyze,
    .name    = "Gemini",
};
