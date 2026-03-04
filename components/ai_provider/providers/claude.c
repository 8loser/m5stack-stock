#include "ai_provider.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ai_claude";

#define CLAUDE_URL      "https://api.anthropic.com/v1/messages"
#define CLAUDE_MODEL    "claude-haiku-4-5-20251001"
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

static esp_err_t claude_analyze(const stock_context_t *ctx,
                                  const char *prompt,
                                  const char *api_key,
                                  ai_analysis_result_t *result)
{
    /* 建構請求 */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", CLAUDE_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", 256);
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", prompt);
    cJSON_AddItemToArray(messages, msg);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    char *resp_buf = malloc(RESP_BUF_SIZE);
    if (!resp_buf) { free(body); return ESP_ERR_NO_MEM; }

    resp_ctx_t resp_ctx = {.buf = resp_buf, .size = RESP_BUF_SIZE - 1, .len = 0};

    esp_http_client_config_t cfg = {
        .url              = CLAUDE_URL,
        .method           = HTTP_METHOD_POST,
        .event_handler    = resp_handler,
        .user_data        = &resp_ctx,
        .timeout_ms       = AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    char auth_header[160];
    snprintf(auth_header, sizeof(auth_header), "%s", api_key);

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-api-key", auth_header);
    esp_http_client_set_header(client, "anthropic-version", "2023-06-01");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "Claude HTTP %d: %s", status, esp_err_to_name(ret));
        free(resp_buf);
        strncpy(result->analysis, "Claude API 請求失敗", AI_ANALYSIS_MAX_LEN - 1);
        result->signal = AI_SIGNAL_UNKNOWN;
        return ESP_FAIL;
    }

    resp_buf[resp_ctx.len] = '\0';

    /* 解析 Claude 回應：content[0].text */
    cJSON *resp = cJSON_Parse(resp_buf);
    free(resp_buf);
    if (!resp) return ESP_FAIL;

    const char *text = NULL;
    cJSON *content = cJSON_GetObjectItem(resp, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON *c0 = cJSON_GetArrayItem(content, 0);
        cJSON *t = cJSON_GetObjectItem(c0, "text");
        if (t && cJSON_IsString(t)) text = t->valuestring;
    }

    if (text) {
        const char *js = strchr(text, '{');
        const char *je = strrchr(text, '}');
        if (js && je && je > js) {
            char tmp[512];
            size_t l = je - js + 1;
            if (l < sizeof(tmp)) {
                memcpy(tmp, js, l);
                tmp[l] = '\0';
                cJSON *ai_json = cJSON_Parse(tmp);
                if (ai_json) {
                    cJSON *sig  = cJSON_GetObjectItem(ai_json, "signal");
                    cJSON *conf = cJSON_GetObjectItem(ai_json, "confidence");
                    cJSON *anal = cJSON_GetObjectItem(ai_json, "analysis");
                    result->signal     = parse_signal(sig ? sig->valuestring : NULL);
                    result->confidence = conf ? conf->valueint : 50;
                    if (anal && cJSON_IsString(anal))
                        strncpy(result->analysis, anal->valuestring, AI_ANALYSIS_MAX_LEN - 1);
                    cJSON_Delete(ai_json);
                }
            }
        }
    }

    cJSON_Delete(resp);
    ESP_LOGI(TAG, "Claude 分析完成: signal=%d confidence=%d",
             result->signal, result->confidence);
    return ESP_OK;
}

static esp_err_t claude_test_key(const char *api_key, int *out_status_code)
{
    if (out_status_code) {
        *out_status_code = 0;
    }
    if (!api_key || api_key[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    resp_ctx_t resp_ctx = {.buf = NULL, .size = 0, .len = 0};
    esp_http_client_config_t cfg = {
        .url = CLAUDE_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = resp_handler,
        .user_data = &resp_ctx,
        .timeout_ms = AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_FAIL;
    }

    const char *body =
        "{\"model\":\"" CLAUDE_MODEL "\",\"max_tokens\":1,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}]}";

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-api-key", api_key);
    esp_http_client_set_header(client, "anthropic-version", "2023-06-01");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (out_status_code) {
        *out_status_code = status;
    }
    return ret;
}

const ai_provider_ops_t claude_ops = {
    .analyze = claude_analyze,
    .test_key = claude_test_key,
    .name    = "Claude",
};
