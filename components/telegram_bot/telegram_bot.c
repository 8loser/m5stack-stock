#include "telegram_bot.h"
#include "app_config.h"
#include "storage.h"
#include "device_server.h"
#include "twse_client.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TG_TASK_STACK 12288
#define TG_TASK_PRIO  3
#define TG_HTTP_BUF_INIT_SIZE 512
#define TG_TOKEN_MAX_LEN 160
#define TG_CHAT_ID_MAX_LEN 40

static const char *TAG = "telegram_bot";

typedef struct {
    char *buf;
    size_t capacity;
    size_t data_len;
    bool overflow;
} tg_http_ctx_t;

static TaskHandle_t s_task_handle = NULL;
static bool s_running = false;
static int64_t s_next_update_id = 0;

static size_t url_encode_component(const char *src, char *dst, size_t dst_size)
{
    static const char *hex = "0123456789ABCDEF";
    size_t di = 0;

    if (!src || !dst || dst_size == 0) {
        return 0;
    }

    for (size_t i = 0; src[i] != '\0' && di + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        bool safe = (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~');
        if (safe) {
            dst[di++] = (char)c;
            continue;
        }
        if (di + 3 >= dst_size) {
            break;
        }
        dst[di++] = '%';
        dst[di++] = hex[(c >> 4) & 0x0F];
        dst[di++] = hex[c & 0x0F];
    }
    dst[di] = '\0';
    return di;
}

static esp_err_t tg_http_event_handler(esp_http_client_event_t *evt)
{
    tg_http_ctx_t *ctx = (tg_http_ctx_t *)evt->user_data;
    if (!ctx || evt->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }

    size_t needed = ctx->data_len + (size_t)evt->data_len + 1;
    if (needed > ctx->capacity) {
        size_t new_cap = ctx->capacity;
        while (new_cap < needed && new_cap < 4096U) {
            new_cap *= 2U;
        }
        if (new_cap < needed || new_cap > 4096U) {
            ctx->overflow = true;
            return ESP_OK;
        }
        char *new_buf = realloc(ctx->buf, new_cap);
        if (!new_buf) {
            ctx->overflow = true;
            return ESP_OK;
        }
        ctx->buf = new_buf;
        ctx->capacity = new_cap;
    }

    memcpy(ctx->buf + ctx->data_len, evt->data, (size_t)evt->data_len);
    ctx->data_len += (size_t)evt->data_len;
    return ESP_OK;
}

static bool is_wifi_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

static esp_err_t tg_http_call(const char *url,
                              esp_http_client_method_t method,
                              const char *content_type,
                              const char *post_data,
                              char **out_body,
                              int *out_status_code)
{
    if (!url || !out_body) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_body = NULL;
    if (out_status_code) {
        *out_status_code = 0;
    }

    tg_http_ctx_t ctx = {0};
    ctx.buf = malloc(TG_HTTP_BUF_INIT_SIZE);
    if (!ctx.buf) {
        return ESP_ERR_NO_MEM;
    }
    ctx.capacity = TG_HTTP_BUF_INIT_SIZE;

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = tg_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(ctx.buf);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, method);
    if (content_type) {
        esp_http_client_set_header(client, "Content-Type", content_type);
    }
    if (post_data) {
        esp_http_client_set_post_field(client, post_data, (int)strlen(post_data));
    }

    esp_err_t ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (out_status_code) {
        *out_status_code = status_code;
    }

    if (ret != ESP_OK || ctx.overflow) {
        free(ctx.buf);
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }
    ctx.buf[ctx.data_len] = '\0';
    *out_body = ctx.buf;
    return ESP_OK;
}

static esp_err_t tg_send_message(const char *token, const char *chat_id, const char *text)
{
    if (!token || !chat_id || !text || token[0] == '\0' || chat_id[0] == '\0' || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char encoded_chat[96] = {0};
    char encoded_text[384] = {0};
    char post_data[560] = {0};
    char url[320] = {0};
    char *resp = NULL;
    int status_code = 0;

    url_encode_component(chat_id, encoded_chat, sizeof(encoded_chat));
    url_encode_component(text, encoded_text, sizeof(encoded_text));
    snprintf(post_data, sizeof(post_data), "chat_id=%s&text=%s", encoded_chat, encoded_text);
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", token);

    esp_err_t ret = tg_http_call(url,
                                 HTTP_METHOD_POST,
                                 "application/x-www-form-urlencoded",
                                 post_data,
                                 &resp,
                                 &status_code);
    if (ret != ESP_OK) {
        return ret;
    }

    bool ok = (status_code == 200 && strstr(resp, "\"ok\":true") != NULL);
    free(resp);
    return ok ? ESP_OK : ESP_FAIL;
}

static void append_fmt(char *buf, size_t buf_size, size_t *used, const char *fmt, ...)
{
    if (!buf || !used || *used >= buf_size) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *used, buf_size - *used, fmt, args);
    va_end(args);

    if (n <= 0) {
        return;
    }
    size_t wrote = (size_t)n;
    if (wrote >= (buf_size - *used)) {
        *used = buf_size - 1;
    } else {
        *used += wrote;
    }
}

static bool is_command(const char *text, const char *cmd)
{
    size_t n;
    if (!text || !cmd) return false;
    n = strlen(cmd);
    if (strncmp(text, cmd, n) != 0) return false;
    return text[n] == '\0' || isspace((unsigned char)text[n]) || text[n] == '@';
}

static void build_info_message(char *out, size_t out_size)
{
    size_t used = 0;
    bool connected = device_server_is_connected();
    const char *ssid = device_server_get_connected_ssid();
    const char *ip = device_server_get_ip();
    stock_list_t stocks = {0};

    if (!out || out_size < 64) {
        return;
    }
    out[0] = '\0';

    append_fmt(out, out_size, &used, "WiFi: %s\n", connected ? "connected" : "disconnected");
    append_fmt(out, out_size, &used, "SSID: %s\n",
               (ssid && ssid[0] != '\0') ? ssid : "N/A");
    append_fmt(out, out_size, &used, "IP: %s\n\n",
               (ip && ip[0] != '\0') ? ip : "N/A");

    if (storage_stocks_load(&stocks) != ESP_OK || stocks.count == 0) {
        append_fmt(out, out_size, &used, "Stocks: none");
        return;
    }

    append_fmt(out, out_size, &used, "Stocks (%u):\n", (unsigned)stocks.count);

    if (!connected) {
        for (uint8_t i = 0; i < stocks.count; i++) {
            append_fmt(out, out_size, &used, "%s: N/A (wifi disconnected)\n", stocks.symbols[i]);
        }
        return;
    }

    stock_quote_t *quotes = calloc(stocks.count, sizeof(stock_quote_t));
    if (!quotes) {
        append_fmt(out, out_size, &used, "quote fetch failed: OOM\n");
        for (uint8_t i = 0; i < stocks.count; i++) {
            append_fmt(out, out_size, &used, "%s: N/A\n", stocks.symbols[i]);
        }
        return;
    }

    esp_err_t ret = twse_client_fetch((const char (*)[8])stocks.symbols, stocks.count, quotes);
    if (ret != ESP_OK) {
        append_fmt(out, out_size, &used, "quote fetch failed: %s\n", esp_err_to_name(ret));
        for (uint8_t i = 0; i < stocks.count; i++) {
            append_fmt(out, out_size, &used, "%s: N/A\n", stocks.symbols[i]);
        }
        free(quotes);
        return;
    }

    for (uint8_t i = 0; i < stocks.count; i++) {
        const char *sym = stocks.symbols[i];
        if (quotes[i].is_valid) {
            append_fmt(out, out_size, &used, "%s %.2f (%+.2f%%)\n",
                       sym,
                       quotes[i].current_price,
                       quotes[i].change_percent);
        } else if (quotes[i].is_market_closed) {
            append_fmt(out, out_size, &used, "%s %.2f (market closed)\n",
                       sym,
                       quotes[i].current_price);
        } else {
            append_fmt(out, out_size, &used, "%s N/A\n", sym);
        }
    }

    free(quotes);
}

static const char *response_for_command(const char *text, char *scratch, size_t scratch_size)
{
    if (!text || text[0] == '\0') {
        return NULL;
    }

    if (is_command(text, "/info")) {
        if (!scratch || scratch_size == 0) {
            return "internal_error";
        }
        build_info_message(scratch, scratch_size);
        return scratch;
    }

    if (is_command(text, "/help")) {
        return "Commands: /info /help";
    }
    return "Unknown command. Use /help";
}

static void process_updates(const char *token, const char *allowed_chat_id, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    int n = cJSON_GetArraySize(result);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(result, i);
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *update_id = cJSON_GetObjectItem(item, "update_id");
        if (cJSON_IsNumber(update_id)) {
            int64_t id = (int64_t)update_id->valuedouble;
            if (id >= s_next_update_id) {
                s_next_update_id = id + 1;
            }
        }

        cJSON *message = cJSON_GetObjectItem(item, "message");
        if (!cJSON_IsObject(message)) {
            continue;
        }
        cJSON *chat = cJSON_GetObjectItem(message, "chat");
        cJSON *text = cJSON_GetObjectItem(message, "text");
        if (!cJSON_IsObject(chat) || !cJSON_IsString(text) || !text->valuestring) {
            continue;
        }

        cJSON *chat_id_obj = cJSON_GetObjectItem(chat, "id");
        char chat_id[TG_CHAT_ID_MAX_LEN] = {0};
        if (cJSON_IsNumber(chat_id_obj)) {
            snprintf(chat_id, sizeof(chat_id), "%.0f", chat_id_obj->valuedouble);
        } else if (cJSON_IsString(chat_id_obj) && chat_id_obj->valuestring) {
            strlcpy(chat_id, chat_id_obj->valuestring, sizeof(chat_id));
        } else {
            continue;
        }

        if (strcmp(chat_id, allowed_chat_id) != 0) {
            ESP_LOGW(TAG, "reject chat_id=%s", chat_id);
            continue;
        }

        bool is_info_cmd = is_command(text->valuestring, "/info");
        char *response_buf = NULL;
        const char *resp = NULL;

        if (is_info_cmd) {
            response_buf = malloc(3072);
            if (!response_buf) {
                resp = "OOM";
            } else {
                resp = response_for_command(text->valuestring, response_buf, 3072);
            }
        } else {
            resp = response_for_command(text->valuestring, NULL, 0);
        }

        if (!resp) {
            free(response_buf);
            continue;
        }
        esp_err_t ret = tg_send_message(token, chat_id, resp);
        free(response_buf);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "send response failed: %s", esp_err_to_name(ret));
        }
    }

    cJSON_Delete(root);
}

static void telegram_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "telegram bot task started");

    while (s_running) {
        bool enabled = false;
        char token[TG_TOKEN_MAX_LEN] = {0};
        char chat_id[TG_CHAT_ID_MAX_LEN] = {0};

        if (storage_tg_load_enabled(&enabled) != ESP_OK || !enabled) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        storage_tg_load_bot_token(token, sizeof(token));
        storage_tg_load_chat_id(chat_id, sizeof(chat_id));
        if (token[0] == '\0' || chat_id[0] == '\0') {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        if (!is_wifi_connected()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        char url[384] = {0};
        if (s_next_update_id > 0) {
            snprintf(url, sizeof(url),
                     "https://api.telegram.org/bot%s/getUpdates?offset=%lld&timeout=20&limit=10",
                     token, (long long)s_next_update_id);
        } else {
            snprintf(url, sizeof(url),
                     "https://api.telegram.org/bot%s/getUpdates?timeout=20&limit=10",
                     token);
        }

        char *resp = NULL;
        int status_code = 0;
        esp_err_t ret = tg_http_call(url, HTTP_METHOD_GET, NULL, NULL, &resp, &status_code);
        if (ret != ESP_OK || !resp || status_code != 200) {
            free(resp);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        process_updates(token, chat_id, resp);
        free(resp);
    }

    ESP_LOGI(TAG, "telegram bot task stopped");
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t telegram_bot_init(void)
{
    return ESP_OK;
}

esp_err_t telegram_bot_start(void)
{
    if (s_running) {
        return ESP_OK;
    }
    s_running = true;
    if (xTaskCreate(telegram_task, "telegram_bot", TG_TASK_STACK, NULL, TG_TASK_PRIO, &s_task_handle) != pdPASS) {
        s_running = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t telegram_bot_stop(void)
{
    s_running = false;
    return ESP_OK;
}

bool telegram_bot_is_running(void)
{
    return s_running;
}
