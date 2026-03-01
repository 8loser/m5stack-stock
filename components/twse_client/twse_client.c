#include "twse_client.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "twse";

#define HTTP_BUF_SIZE   4096
#define MAX_SYMBOLS     10

static QueueHandle_t s_queue        = NULL;
static TaskHandle_t  s_task_handle  = NULL;
static bool          s_task_running = false;

/* HTTP 事件回調，用於接收 body */
typedef struct {
    char   *buf;
    size_t  buf_size;
    size_t  data_len;
    bool    overflow;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->data_len + evt->data_len < ctx->buf_size) {
            memcpy(ctx->buf + ctx->data_len, evt->data, evt->data_len);
            ctx->data_len += evt->data_len;
        } else {
            ctx->overflow = true;
        }
    }
    return ESP_OK;
}

/* 解析單一股票 JSON 物件 */
static void parse_stock_item(cJSON *item, const char *symbol, stock_quote_t *q)
{
    strncpy(q->symbol, symbol, 7);
    q->is_valid = false;
    q->is_market_closed = false;

    cJSON *name = cJSON_GetObjectItem(item, "n");
    if (name && cJSON_IsString(name)) {
        strncpy(q->name, name->valuestring, 31);
    }

    /* 現價 z */
    cJSON *price = cJSON_GetObjectItem(item, "z");
    if (!price || !cJSON_IsString(price) ||
        strcmp(price->valuestring, "-") == 0) {
        /* 休市 */
        q->is_market_closed = true;
        /* 嘗試讀昨收 */
        cJSON *yday = cJSON_GetObjectItem(item, "y");
        if (yday && cJSON_IsString(yday) &&
            strcmp(yday->valuestring, "-") != 0) {
            q->current_price = strtof(yday->valuestring, NULL);
            q->yesterday_close = q->current_price;
        }
        return;
    }

    q->current_price = strtof(price->valuestring, NULL);

#define PARSE_FLOAT(key, field) do { \
    cJSON *_j = cJSON_GetObjectItem(item, key); \
    if (_j && cJSON_IsString(_j) && strcmp(_j->valuestring, "-") != 0) \
        q->field = strtof(_j->valuestring, NULL); \
} while(0)

    PARSE_FLOAT("o", open_price);
    PARSE_FLOAT("h", high_price);
    PARSE_FLOAT("l", low_price);
    PARSE_FLOAT("y", yesterday_close);

    cJSON *vol = cJSON_GetObjectItem(item, "v");
    if (vol && cJSON_IsString(vol) && strcmp(vol->valuestring, "-") != 0) {
        q->volume = strtol(vol->valuestring, NULL, 10);
    }

    cJSON *t = cJSON_GetObjectItem(item, "t");
    if (t && cJSON_IsString(t)) {
        strncpy(q->trade_time, t->valuestring, 15);
    }

    q->change_amount  = q->current_price - q->yesterday_close;
    q->change_percent = (q->yesterday_close > 0.0f)
                        ? (q->change_amount / q->yesterday_close * 100.0f)
                        : 0.0f;
    q->is_valid = true;
}

esp_err_t twse_client_fetch(const char symbols[][8], uint8_t count,
                             stock_quote_t *results)
{
    /* 組合 ex_ch 參數：tse_2330.tw|tse_2317.tw|... */
    char ex_ch[256] = {0};
    for (int i = 0; i < count; i++) {
        if (i > 0) strlcat(ex_ch, "|", sizeof(ex_ch));
        char tmp[20];
        snprintf(tmp, sizeof(tmp), "tse_%s.tw", symbols[i]);
        strlcat(ex_ch, tmp, sizeof(ex_ch));
    }

    char url[512];
    snprintf(url, sizeof(url),
             "%s?ex_ch=%s&json=1&delay=0", TWSE_BASE_URL, ex_ch);

    char *buf = malloc(HTTP_BUF_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

    http_ctx_t ctx = {.buf = buf, .buf_size = HTTP_BUF_SIZE - 1, .data_len = 0};

    esp_http_client_config_t cfg = {
        .url            = url,
        .event_handler  = http_event_handler,
        .user_data      = &ctx,
        .timeout_ms     = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t ret = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || ctx.overflow) {
        ESP_LOGE(TAG, "HTTP 請求失敗: %s", esp_err_to_name(ret));
        free(buf);
        return ESP_FAIL;
    }

    buf[ctx.data_len] = '\0';
    ESP_LOGD(TAG, "TWSE 回應: %.200s...", buf);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "JSON 解析失敗");
        return ESP_FAIL;
    }

    cJSON *msg_array = cJSON_GetObjectItem(root, "msgArray");
    if (!cJSON_IsArray(msg_array)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int n = cJSON_GetArraySize(msg_array);
    for (int i = 0; i < n && i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(msg_array, i);
        cJSON *sym = cJSON_GetObjectItem(item, "c");
        const char *sym_str = (sym && cJSON_IsString(sym))
                              ? sym->valuestring : symbols[i];
        parse_stock_item(item, sym_str, &results[i]);

        if (results[i].is_valid) {
            ESP_LOGI(TAG, "%s %s %.2f (%.2f%%)",
                     results[i].symbol, results[i].name,
                     results[i].current_price, results[i].change_percent);
        } else if (results[i].is_market_closed) {
            ESP_LOGI(TAG, "%s 休市，昨收 %.2f",
                     results[i].symbol, results[i].yesterday_close);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

typedef struct {
    char    symbols[MAX_SYMBOLS][8];
    uint8_t count;
    uint32_t interval_s;
} fetch_task_args_t;

static void twse_fetch_task(void *arg)
{
    fetch_task_args_t *args = (fetch_task_args_t *)arg;
    stock_quote_t results[MAX_SYMBOLS];

    while (s_task_running) {
        memset(results, 0, sizeof(results));
        esp_err_t ret = twse_client_fetch(args->symbols, args->count, results);
        if (ret == ESP_OK && s_queue) {
            for (int i = 0; i < args->count; i++) {
                xQueueOverwrite(s_queue, &results[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(args->interval_s * 1000));
    }

    free(args);
    vTaskDelete(NULL);
}

esp_err_t twse_client_init(QueueHandle_t quote_queue)
{
    s_queue = quote_queue;
    ESP_LOGI(TAG, "TWSE Client 初始化完成");
    return ESP_OK;
}

esp_err_t twse_client_start_task(const char symbols[][8], uint8_t count,
                                  uint32_t interval_s)
{
    if (s_task_running) twse_client_stop_task();

    fetch_task_args_t *args = malloc(sizeof(fetch_task_args_t));
    if (!args) return ESP_ERR_NO_MEM;

    args->count = count;
    args->interval_s = interval_s;
    memcpy(args->symbols, symbols, count * 8);

    s_task_running = true;
    BaseType_t res = xTaskCreatePinnedToCore(twse_fetch_task, "twse_fetch",
                                              STACK_TWSE, args,
                                              TASK_PRIO_TWSE,
                                              &s_task_handle, 0);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

void twse_client_stop_task(void)
{
    s_task_running = false;
    if (s_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_task_handle = NULL;
    }
}
