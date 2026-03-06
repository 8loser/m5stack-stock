#include "telegram_bot.h"
#include "app_config.h"
#include "storage.h"
#include "network_portal.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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
#define TG_INFO_STALE_SEC 1800
#define TG_GET_UPDATES_TIMEOUT_SEC 3
#define TG_RESTART_WAIT_MS 3000U
#define TG_SEND_RETRY_MAX 3
#define TG_SEND_RETRY_DELAY_MS 250U
#define TG_INFO_MSG_BUF_SIZE 2048U

static const char *TAG = "telegram_bot";

typedef struct {
    char *buf;
    size_t capacity;
    size_t data_len;
    bool overflow;
} tg_http_ctx_t;

static TaskHandle_t s_task_handle = NULL;
static bool s_running = false;
static bool s_polling_paused = false;
static int64_t s_next_update_id = 0;
static portMUX_TYPE s_http_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_http_in_flight = 0;
static esp_http_client_handle_t s_http_client = NULL;
static volatile bool s_http_abort_requested = false;

typedef struct {
    bool has_quote;
    stock_quote_t quote;
    int64_t updated_at_s;
} tg_quote_cache_entry_t;

static tg_quote_cache_entry_t s_quote_cache[MAX_STOCK_COUNT];
static char s_last_quote_trade_time[sizeof(((stock_quote_t *)0)->trade_time)];
static int64_t s_last_quote_update_s = 0;
static SemaphoreHandle_t s_cache_mutex = NULL;

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

    taskENTER_CRITICAL(&s_http_lock);
    s_http_client = client;
    s_http_in_flight++;
    s_http_abort_requested = false;
    taskEXIT_CRITICAL(&s_http_lock);
    esp_err_t ret = esp_http_client_perform(client);
    bool abort_requested = false;
    taskENTER_CRITICAL(&s_http_lock);
    abort_requested = s_http_abort_requested;
    taskEXIT_CRITICAL(&s_http_lock);
    if (abort_requested && ret != ESP_OK) {
        ESP_LOGI(TAG, "http call aborted: %s", esp_err_to_name(ret));
    }
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    taskENTER_CRITICAL(&s_http_lock);
    s_http_client = NULL;
    if (s_http_in_flight > 0) {
        s_http_in_flight--;
    }
    taskEXIT_CRITICAL(&s_http_lock);
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
    char url[320] = {0};
    char *encoded_text = NULL;
    char *post_data = NULL;
    char *resp = NULL;
    int status_code = 0;
    esp_err_t ret = ESP_FAIL;

    url_encode_component(chat_id, encoded_chat, sizeof(encoded_chat));
    size_t text_len = strlen(text);
    size_t encoded_text_cap = (text_len * 3U) + 1U;
    encoded_text = malloc(encoded_text_cap);
    if (!encoded_text) {
        return ESP_ERR_NO_MEM;
    }
    url_encode_component(text, encoded_text, encoded_text_cap);

    size_t post_data_len = strlen("chat_id=&text=") + strlen(encoded_chat) + strlen(encoded_text) + 1U;
    post_data = malloc(post_data_len);
    if (!post_data) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    snprintf(post_data, post_data_len, "chat_id=%s&text=%s", encoded_chat, encoded_text);
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", token);

    for (int attempt = 1; attempt <= TG_SEND_RETRY_MAX; attempt++) {
        free(resp);
        resp = NULL;
        status_code = 0;
        ret = tg_http_call(url,
                           HTTP_METHOD_POST,
                           "application/x-www-form-urlencoded",
                           post_data,
                           &resp,
                           &status_code);
        if (ret == ESP_OK) {
            bool ok = (status_code == 200 && resp && strstr(resp, "\"ok\":true") != NULL);
            if (ok) {
                break;
            }
            ret = ESP_FAIL;
        }

        bool retryable = (ret == ESP_ERR_HTTP_CONNECT ||
                          ret == ESP_ERR_NO_MEM ||
                          status_code == 0 ||
                          status_code >= 500);
        if (!retryable || attempt >= TG_SEND_RETRY_MAX) {
            ESP_LOGW(TAG, "sendMessage failed attempt=%d ret=%s status=%d",
                     attempt, esp_err_to_name(ret), status_code);
            break;
        }
        ESP_LOGW(TAG, "sendMessage retry attempt=%d ret=%s status=%d",
                 attempt, esp_err_to_name(ret), status_code);
        vTaskDelay(pdMS_TO_TICKS(TG_SEND_RETRY_DELAY_MS));
    }

cleanup:
    free(resp);
    free(post_data);
    free(encoded_text);
    return ret;
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

static int64_t now_sec(void)
{
    return esp_timer_get_time() / 1000000LL;
}

static tg_quote_cache_entry_t *find_cache_entry_locked(const char *symbol, bool create)
{
    tg_quote_cache_entry_t *empty = NULL;

    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (!s_quote_cache[i].has_quote) {
            if (!empty) {
                empty = &s_quote_cache[i];
            }
            continue;
        }
        if (strncmp(s_quote_cache[i].quote.symbol, symbol, sizeof(s_quote_cache[i].quote.symbol)) == 0) {
            return &s_quote_cache[i];
        }
    }

    if (!create || !empty) {
        return NULL;
    }
    memset(empty, 0, sizeof(*empty));
    return empty;
}

static bool cache_get_quote_snapshot(const char *symbol, stock_quote_t *out_quote, int64_t *out_updated_at_s)
{
    bool found = false;

    if (!symbol || !out_quote || !out_updated_at_s || !s_cache_mutex) {
        return false;
    }

    if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    tg_quote_cache_entry_t *entry = find_cache_entry_locked(symbol, false);
    if (entry) {
        *out_quote = entry->quote;
        *out_updated_at_s = entry->updated_at_s;
        found = true;
    }
    xSemaphoreGive(s_cache_mutex);
    return found;
}

static bool cache_get_last_update(char *trade_time, size_t trade_time_size, int64_t *out_last_update_s)
{
    bool available = false;

    if (!trade_time || trade_time_size == 0 || !out_last_update_s || !s_cache_mutex) {
        return false;
    }

    if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    *out_last_update_s = s_last_quote_update_s;
    strlcpy(trade_time, s_last_quote_trade_time, trade_time_size);
    available = (s_last_quote_update_s > 0);
    xSemaphoreGive(s_cache_mutex);
    return available;
}

esp_err_t telegram_bot_cache_quote(const stock_quote_t *quote)
{
    if (!quote || quote->symbol[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_cache_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    tg_quote_cache_entry_t *entry = find_cache_entry_locked(quote->symbol, true);
    if (!entry) {
        xSemaphoreGive(s_cache_mutex);
        return ESP_ERR_NO_MEM;
    }

    entry->quote = *quote;
    entry->updated_at_s = now_sec();
    entry->has_quote = true;

    s_last_quote_update_s = entry->updated_at_s;
    if (quote->trade_time[0] != '\0') {
        strlcpy(s_last_quote_trade_time, quote->trade_time, sizeof(s_last_quote_trade_time));
    } else {
        s_last_quote_trade_time[0] = '\0';
    }

    xSemaphoreGive(s_cache_mutex);
    return ESP_OK;
}

static void build_info_message(char *out, size_t out_size)
{
    size_t used = 0;
    bool connected = network_portal_is_connected();
    const char *ssid = network_portal_get_connected_ssid();
    const char *ip = network_portal_get_ip();
    stock_list_t stocks = {0};
    int64_t last_quote_update_s = 0;
    char last_trade_time[16] = {0};
    bool has_last_update = false;

    if (!out || out_size < 64) {
        return;
    }
    out[0] = '\0';

    append_fmt(out, out_size, &used, "WiFi: %s\n", connected ? "connected" : "disconnected");
    append_fmt(out, out_size, &used, "SSID: %s\n",
               (ssid && ssid[0] != '\0') ? ssid : "N/A");
    append_fmt(out, out_size, &used, "IP: %s\n\n",
               (ip && ip[0] != '\0') ? ip : "N/A");

    has_last_update = cache_get_last_update(last_trade_time, sizeof(last_trade_time), &last_quote_update_s);
    if (has_last_update && last_trade_time[0] != '\0') {
        append_fmt(out, out_size, &used, "Last quote update: %s\n\n", last_trade_time);
    } else if (has_last_update) {
        append_fmt(out, out_size, &used, "Last quote update: %llds ago\n\n",
                   (long long)(now_sec() - last_quote_update_s));
    } else {
        append_fmt(out, out_size, &used, "Last quote update: N/A\n\n");
    }

    if (storage_stocks_load(&stocks) != ESP_OK || stocks.count == 0) {
        append_fmt(out, out_size, &used, "Stocks: none");
        return;
    }

    append_fmt(out, out_size, &used, "Stocks (%u):\n", (unsigned)stocks.count);

    for (uint8_t i = 0; i < stocks.count; i++) {
        const char *sym = stocks.symbols[i];
        stock_meta_t meta = {0};
        const char *name = "N/A";
        stock_quote_t quote = {0};
        int64_t updated_at_s = 0;
        bool stale = false;
        bool has_quote = cache_get_quote_snapshot(sym, &quote, &updated_at_s);
        if (storage_stock_meta_load(sym, &meta) == ESP_OK && meta.name[0] != '\0') {
            name = meta.name;
        }

        if (!has_quote || (!quote.is_valid && !quote.is_market_closed)) {
            append_fmt(out, out_size, &used, "%s %s: N/A | updated N/A\n", sym, name);
            continue;
        }

        stale = ((now_sec() - updated_at_s) > TG_INFO_STALE_SEC);

        if (quote.is_market_closed) {
            append_fmt(out, out_size, &used, "%s %s: %.2f (market closed) | updated %s%s\n",
                       sym, name, quote.current_price,
                       (quote.trade_time[0] != '\0') ? quote.trade_time : "N/A",
                       stale ? " (stale)" : "");
            continue;
        }

        append_fmt(out, out_size, &used, "%s %s: %.2f (%+.2f%%) | updated %s%s\n",
                   sym, name, quote.current_price, quote.change_percent,
                   (quote.trade_time[0] != '\0') ? quote.trade_time : "N/A",
                   stale ? " (stale)" : "");
    }
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

static char *alloc_info_response_buffer(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    char *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) {
        return buf;
    }
    return malloc(size);
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
            response_buf = alloc_info_response_buffer(TG_INFO_MSG_BUF_SIZE);
            if (!response_buf) {
                resp = "OOM";
            } else {
                resp = response_for_command(text->valuestring, response_buf, TG_INFO_MSG_BUF_SIZE);
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

static esp_err_t tg_parse_update_window(const char *json, int64_t *out_max_id, int *out_count)
{
    if (!json || !out_max_id || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_max_id = -1;
    *out_count = 0;
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int n = cJSON_GetArraySize(result);
    *out_count = n;
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(result, i);
        if (!cJSON_IsObject(item)) {
            continue;
        }
        cJSON *update_id = cJSON_GetObjectItem(item, "update_id");
        if (!cJSON_IsNumber(update_id)) {
            continue;
        }
        int64_t id = (int64_t)update_id->valuedouble;
        if (id > *out_max_id) {
            *out_max_id = id;
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t tg_bootstrap_sync_next_update_id(const char *token)
{
    if (!token || token[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t max_update_id = -1;
    int64_t offset = 0;
    esp_err_t ret = ESP_OK;

    while (1) {
        char url[384] = {0};
        char *resp = NULL;
        int status_code = 0;
        int count = 0;
        int64_t batch_max_update_id = -1;

        snprintf(url, sizeof(url),
                 "https://api.telegram.org/bot%s/getUpdates?offset=%lld&timeout=0&limit=100",
                 token,
                 (long long)offset);

        ret = tg_http_call(url, HTTP_METHOD_GET, NULL, NULL, &resp, &status_code);
        if (ret != ESP_OK || !resp || status_code != 200) {
            free(resp);
            return ESP_FAIL;
        }

        ret = tg_parse_update_window(resp, &batch_max_update_id, &count);
        free(resp);
        if (ret != ESP_OK) {
            return ret;
        }

        if (count <= 0) {
            break;
        }
        if (batch_max_update_id > max_update_id) {
            max_update_id = batch_max_update_id;
        }
        if (batch_max_update_id < 0) {
            break;
        }

        offset = batch_max_update_id + 1;
        if (count < 100) {
            break;
        }
    }

    s_next_update_id = (max_update_id >= 0) ? (max_update_id + 1) : offset;
    ESP_LOGI(TAG, "bootstrap sync done next_update_id=%lld", (long long)s_next_update_id);
    return ESP_OK;
}

static void telegram_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "telegram bot task started");
    bool bootstrap_done = false;

    while (s_running) {
        if (s_polling_paused) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        bool enabled = false;
        char token[TG_TOKEN_MAX_LEN] = {0};
        char chat_id[TG_CHAT_ID_MAX_LEN] = {0};

        if (storage_tg_load_enabled(&enabled) != ESP_OK || !enabled) {
            bootstrap_done = false;
            s_next_update_id = 0;
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        storage_tg_load_bot_token(token, sizeof(token));
        storage_tg_load_chat_id(chat_id, sizeof(chat_id));
        if (token[0] == '\0' || chat_id[0] == '\0') {
            bootstrap_done = false;
            s_next_update_id = 0;
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        if (!is_wifi_connected()) {
            bootstrap_done = false;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        if (!bootstrap_done) {
            esp_err_t bootstrap_ret = tg_bootstrap_sync_next_update_id(token);
            if (bootstrap_ret != ESP_OK) {
                ESP_LOGW(TAG, "bootstrap sync failed: %s", esp_err_to_name(bootstrap_ret));
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
            bootstrap_done = true;
        }

        char url[384] = {0};
        snprintf(url, sizeof(url),
                 "https://api.telegram.org/bot%s/getUpdates?offset=%lld&timeout=%d&limit=10",
                 token, (long long)s_next_update_id, TG_GET_UPDATES_TIMEOUT_SEC);

        char *resp = NULL;
        int status_code = 0;
        esp_err_t ret = tg_http_call(url, HTTP_METHOD_GET, NULL, NULL, &resp, &status_code);
        if (ret != ESP_OK || !resp || status_code != 200) {
            ESP_LOGW(TAG, "polling failed: ret=%s status=%d", esp_err_to_name(ret), status_code);
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
    if (s_cache_mutex == NULL) {
        s_cache_mutex = xSemaphoreCreateMutex();
        if (s_cache_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    memset(s_quote_cache, 0, sizeof(s_quote_cache));
    memset(s_last_quote_trade_time, 0, sizeof(s_last_quote_trade_time));
    s_last_quote_update_s = 0;
    s_polling_paused = false;
    s_next_update_id = 0;
    return ESP_OK;
}

esp_err_t telegram_bot_start(void)
{
    if (s_running) {
        ESP_LOGI(TAG, "telegram bot already running");
        return ESP_OK;
    }
    if (s_task_handle != NULL) {
        esp_err_t wait_ret = telegram_bot_wait_stopped(TG_RESTART_WAIT_MS);
        if (wait_ret != ESP_OK) {
            ESP_LOGW(TAG, "telegram bot restart blocked: previous task still stopping (%s)",
                     esp_err_to_name(wait_ret));
            telegram_bot_stop();
            wait_ret = telegram_bot_wait_stopped(TG_RESTART_WAIT_MS);
            if (wait_ret != ESP_OK) {
                ESP_LOGW(TAG, "telegram bot restart failed after retry: %s",
                         esp_err_to_name(wait_ret));
                return wait_ret;
            }
        }
    }
    s_polling_paused = false;
    s_running = true;
    if (xTaskCreate(telegram_task, "telegram_bot", TG_TASK_STACK, NULL, TG_TASK_PRIO, &s_task_handle) != pdPASS) {
        s_running = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "telegram bot start requested");
    return ESP_OK;
}

esp_err_t telegram_bot_stop(void)
{
    esp_http_client_handle_t client = NULL;
    bool has_in_flight = false;

    s_running = false;
    s_polling_paused = false;
    taskENTER_CRITICAL(&s_http_lock);
    s_http_abort_requested = true;
    client = s_http_client;
    has_in_flight = (s_http_in_flight > 0);
    taskEXIT_CRITICAL(&s_http_lock);

    if (has_in_flight && client) {
        esp_err_t abort_ret = esp_http_client_close(client);
        if (abort_ret != ESP_OK) {
            ESP_LOGW(TAG, "telegram http abort failed: %s", esp_err_to_name(abort_ret));
        } else {
            ESP_LOGI(TAG, "telegram stop requested: aborted in-flight http");
        }
    } else {
        ESP_LOGI(TAG, "telegram stop requested: no in-flight http");
    }
    return ESP_OK;
}

esp_err_t telegram_bot_pause_polling(void)
{
    s_polling_paused = true;
    return ESP_OK;
}

esp_err_t telegram_bot_resume_polling(void)
{
    s_polling_paused = false;
    return ESP_OK;
}

bool telegram_bot_is_running(void)
{
    return s_running;
}

bool telegram_bot_is_polling_paused(void)
{
    return s_polling_paused;
}

bool telegram_bot_is_http_in_flight(void)
{
    bool in_flight = false;
    taskENTER_CRITICAL(&s_http_lock);
    in_flight = (s_http_in_flight > 0);
    taskEXIT_CRITICAL(&s_http_lock);
    return in_flight;
}

esp_err_t telegram_bot_wait_http_idle(uint32_t timeout_ms)
{
    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ms * 1000LL;

    while (telegram_bot_is_http_in_flight()) {
        if ((esp_timer_get_time() - start_us) >= timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

esp_err_t telegram_bot_send_text(const char *text)
{
    if (!text || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    bool enabled = false;
    if (storage_tg_load_enabled(&enabled) != ESP_OK || !enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    char token[TG_TOKEN_MAX_LEN] = {0};
    char chat_id[TG_CHAT_ID_MAX_LEN] = {0};
    storage_tg_load_bot_token(token, sizeof(token));
    storage_tg_load_chat_id(chat_id, sizeof(chat_id));
    if (token[0] == '\0' || chat_id[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    return tg_send_message(token, chat_id, text);
}

esp_err_t telegram_bot_wait_stopped(uint32_t timeout_ms)
{
    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ms * 1000LL;

    while (s_task_handle != NULL || telegram_bot_is_http_in_flight()) {
        if ((esp_timer_get_time() - start_us) >= timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}
