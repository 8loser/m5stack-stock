#include "ai_provider.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ai_openai";

#define OPENAI_URL      "https://api.openai.com/v1/chat/completions"
#define OPENAI_MODEL    "gpt-4o-mini"
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

static esp_err_t openai_analyze(const stock_context_t *ctx,
                                  const char *prompt,
                                  const char *api_key,
                                  ai_analysis_result_t *result)
{
    /* 建構請求 */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", OPENAI_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", 256);
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");

    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", "你是台灣股市分析師，請以 JSON 格式回應。");
    cJSON_AddItemToArray(messages, sys_msg);

    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", prompt);
    cJSON_AddItemToArray(messages, user_msg);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    char *resp_buf = malloc(RESP_BUF_SIZE);
    if (!resp_buf) { free(body); return ESP_ERR_NO_MEM; }

    resp_ctx_t resp_ctx = {.buf = resp_buf, .size = RESP_BUF_SIZE - 1, .len = 0};

    esp_http_client_config_t cfg = {
        .url              = OPENAI_URL,
        .method           = HTTP_METHOD_POST,
        .event_handler    = resp_handler,
        .user_data        = &resp_ctx,
        .timeout_ms       = AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    char auth_header[160];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "OpenAI HTTP %d: %s", status, esp_err_to_name(ret));
        free(resp_buf);
        strncpy(result->analysis, "OpenAI API 請求失敗", AI_ANALYSIS_MAX_LEN - 1);
        result->signal = AI_SIGNAL_UNKNOWN;
        return ESP_FAIL;
    }

    resp_buf[resp_ctx.len] = '\0';

    /* 解析：choices[0].message.content */
    cJSON *resp = cJSON_Parse(resp_buf);
    free(resp_buf);
    if (!resp) return ESP_FAIL;

    const char *text = NULL;
    cJSON *choices = cJSON_GetObjectItem(resp, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *c0 = cJSON_GetArrayItem(choices, 0);
        cJSON *msg_obj = cJSON_GetObjectItem(c0, "message");
        cJSON *t = cJSON_GetObjectItem(msg_obj, "content");
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
    ESP_LOGI(TAG, "OpenAI 分析完成: signal=%d confidence=%d",
             result->signal, result->confidence);
    return ESP_OK;
}

const ai_provider_ops_t openai_ops = {
    .analyze = openai_analyze,
    .name    = "OpenAI",
};
