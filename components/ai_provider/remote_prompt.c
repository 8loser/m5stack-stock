#include "ai_provider.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "remote_prompt";

#define REMOTE_BUF_SIZE  8192  /* 遠端 Prompt 最大 8KB */

static remote_prompt_config_t s_config = {0};

/* HTTP 接收 buffer */
typedef struct {
    char  *buf;
    size_t buf_size;
    size_t len;
} dl_ctx_t;

static esp_err_t dl_event_handler(esp_http_client_event_t *evt)
{
    dl_ctx_t *ctx = (dl_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->len + evt->data_len < ctx->buf_size) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
        }
    }
    return ESP_OK;
}

esp_err_t ai_provider_fetch_remote_prompt(const char *url)
{
    memset(&s_config, 0, sizeof(s_config));

    if (!url || strlen(url) == 0) {
        ESP_LOGI(TAG, "遠端 Prompt 已清除");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "下載遠端 Prompt: %s", url);

    char *buf = malloc(REMOTE_BUF_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

    dl_ctx_t ctx = {.buf = buf, .buf_size = REMOTE_BUF_SIZE - 1, .len = 0};

    esp_http_client_config_t cfg = {
        .url               = url,
        .event_handler     = dl_event_handler,
        .user_data         = &ctx,
        .timeout_ms        = AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = false, /* 允許自動跟隨 30x */
        .max_redirection_count = 3,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "下載失敗 HTTP %d: %s", status, esp_err_to_name(ret));
        free(buf);
        return ESP_FAIL;
    }

    buf[ctx.len] = '\0';
    ESP_LOGD(TAG, "遠端 Prompt 內容: %.300s", buf);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "遠端 Prompt JSON 解析失敗");
        return ESP_FAIL;
    }

    /* system_prompt */
    cJSON *sp = cJSON_GetObjectItem(root, "system_prompt");
    if (sp && cJSON_IsString(sp)) {
        strncpy(s_config.system_prompt, sp->valuestring,
                sizeof(s_config.system_prompt) - 1);
    }

    /* extra_stocks：["2330", "2317", ...] */
    cJSON *stocks = cJSON_GetObjectItem(root, "extra_stocks");
    if (cJSON_IsArray(stocks)) {
        int n = cJSON_GetArraySize(stocks);
        s_config.extra_stock_count = 0;
        for (int i = 0; i < n && i < 10; i++) {
            cJSON *sym = cJSON_GetArrayItem(stocks, i);
            if (cJSON_IsString(sym)) {
                strncpy(s_config.extra_stocks[s_config.extra_stock_count++],
                        sym->valuestring, 7);
            }
        }
        ESP_LOGI(TAG, "遠端指定額外股票 %d 支", s_config.extra_stock_count);
    }

    /* signal_sources */
    cJSON *sources = cJSON_GetObjectItem(root, "signal_sources");
    if (cJSON_IsArray(sources)) {
        int n = cJSON_GetArraySize(sources);
        s_config.signal_source_count = 0;
        for (int i = 0; i < n && i < 5; i++) {
            cJSON *src = cJSON_GetArrayItem(sources, i);
            if (cJSON_IsString(src)) {
                strncpy(s_config.signal_sources[s_config.signal_source_count++],
                        src->valuestring, 31);
            }
        }
    }

    /* analysis_template（覆蓋內建 prompt）*/
    cJSON *tmpl = cJSON_GetObjectItem(root, "analysis_template");
    if (tmpl && cJSON_IsString(tmpl)) {
        strncpy(s_config.analysis_template, tmpl->valuestring,
                sizeof(s_config.analysis_template) - 1);
        ESP_LOGI(TAG, "使用遠端自訂 analysis_template");
    }

    s_config.is_loaded = true;
    cJSON_Delete(root);

    ESP_LOGI(TAG, "遠端 Prompt 載入完成（%d 支額外股票，%d 個訊號來源）",
             s_config.extra_stock_count, s_config.signal_source_count);
    return ESP_OK;
}

const remote_prompt_config_t *ai_provider_get_remote_prompt(void)
{
    return &s_config;
}
