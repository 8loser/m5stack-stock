#include "stock_admin_service.h"
#include "storage.h"
#include "scheduler_service.h"
#include "twse_client.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTAL_BODY_MAX_LEN 4096

#define ERR_INVALID_FORMAT       "invalid_format"
#define ERR_DUPLICATE_SYMBOL     "duplicate_symbol"
#define ERR_LIMIT_EXCEEDED       "limit_exceeded"
#define ERR_NOT_FOUND_OR_NOT_TSE "not_found_or_not_tse"
#define ERR_VALIDATE_FAILED      "validate_failed"
#define ERR_NOT_FOUND            "not_found"
#define ERR_INVALID_THRESHOLD    "invalid_threshold"
#define ERR_PROMPT_TOO_LONG      "prompt_too_long"

#define ALERT_PROMPT_MAX_BYTES   512
#define ALERT_THRESHOLD_MAX      99.99f

typedef struct {
    char symbol[8];
    char name[64];
    char abbr[32];
    char industry[32];
} stock_meta_cache_t;

typedef struct {
    char symbol[8];
    bool has_quote;
    bool available;
    bool is_market_closed;
    float price;
    float change_percent;
    uint8_t limit_status;
} stock_quote_cache_t;

enum {
    QUOTE_LIMIT_UNKNOWN = 0,
    QUOTE_LIMIT_NONE = 1,
    QUOTE_LIMIT_UP = 2,
    QUOTE_LIMIT_DOWN = 3,
};

static const char *TAG = "stock_admin";
static stock_list_changed_cb_t s_stock_list_changed_cb = NULL;
static stock_meta_cache_t s_stock_meta_cache[MAX_STOCK_COUNT];
static stock_quote_cache_t s_stock_quote_cache[MAX_STOCK_COUNT];
static portMUX_TYPE s_quote_cache_mux = portMUX_INITIALIZER_UNLOCKED;

static bool float_nearly_equal(float a, float b)
{
    return fabsf(a - b) <= 0.0005f;
}

static void url_decode(char *dst, size_t dst_len, const char *src)
{
    size_t di = 0;
    if (!dst || dst_len == 0) {
        return;
    }
    dst[0] = '\0';

    for (size_t i = 0; src && src[i] != '\0' && di + 1 < dst_len; i++) {
        if (src[i] == '+') {
            dst[di++] = ' ';
            continue;
        }
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            dst[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
            continue;
        }
        dst[di++] = src[i];
    }

    dst[di] = '\0';
}

static bool get_form_value(const char *body, const char *key, char *out, size_t out_len)
{
    if (!body || !key || !out || out_len == 0) {
        return false;
    }

    size_t key_len = strlen(key);
    const char *p = body;
    while (p && *p) {
        if ((p == body || p[-1] == '&') && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            const char *end = strchr(p, '&');
            size_t raw_len = end ? (size_t)(end - p) : strlen(p);

            if (raw_len > PORTAL_BODY_MAX_LEN) {
                raw_len = PORTAL_BODY_MAX_LEN;
            }
            char *tmp = malloc(raw_len + 1);
            if (!tmp) {
                return false;
            }
            memcpy(tmp, p, raw_len);
            tmp[raw_len] = '\0';
            url_decode(out, out_len, tmp);
            free(tmp);
            return true;
        }

        p = strchr(p, '&');
        if (p) {
            p++;
        }
    }

    return false;
}

static void json_escape(char *out, size_t out_len, const char *in)
{
    size_t di = 0;
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (!in) {
        return;
    }

    for (size_t i = 0; in[i] && di + 2 < out_len; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            if (di + 3 >= out_len) {
                break;
            }
            out[di++] = '\\';
            out[di++] = c;
        } else if (c == '\n') {
            if (di + 3 >= out_len) {
                break;
            }
            out[di++] = '\\';
            out[di++] = 'n';
        } else if (c == '\r') {
            if (di + 3 >= out_len) {
                break;
            }
            out[di++] = '\\';
            out[di++] = 'r';
        } else if (c == '\t') {
            if (di + 3 >= out_len) {
                break;
            }
            out[di++] = '\\';
            out[di++] = 't';
        } else {
            out[di++] = c;
        }
    }
    out[di] = '\0';
}

static esp_err_t read_request_body_alloc(httpd_req_t *req, char **out_body)
{
    if (!req || !out_body) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_body = NULL;
    if (req->content_len <= 0 || req->content_len > PORTAL_BODY_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    char *body = malloc((size_t)req->content_len + 1);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }

    int remaining = req->content_len;
    int offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, body + offset, remaining);
        if (recv_len <= 0) {
            free(body);
            return ESP_FAIL;
        }
        offset += recv_len;
        remaining -= recv_len;
    }

    body[offset] = '\0';
    *out_body = body;
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, int status, const char *json)
{
    httpd_resp_set_status(req, status == 200 ? "200 OK"
                          : status == 400 ? "400 Bad Request"
                          : status == 404 ? "404 Not Found"
                          : status == 409 ? "409 Conflict"
                          : status == 502 ? "502 Bad Gateway"
                          : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_sendstr(req, json ? json : "{}");
}

static esp_err_t send_json_error(httpd_req_t *req, int status, const char *code)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", code ? code : "internal_error");
    return send_json_response(req, status, buf);
}

static bool is_sta_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

static bool is_symbol_format_valid(const char *symbol)
{
    if (!symbol || strlen(symbol) != 4) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)symbol[i])) {
            return false;
        }
    }
    return true;
}

static int stock_list_find_symbol(const stock_list_t *list, const char *symbol)
{
    if (!list || !symbol) {
        return -1;
    }
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i], symbol) == 0) {
            return i;
        }
    }
    return -1;
}

static bool stock_list_add_symbol(stock_list_t *list, const char *symbol)
{
    if (!list || !symbol || list->count >= MAX_STOCK_COUNT) {
        return false;
    }
    strlcpy(list->symbols[list->count], symbol, sizeof(list->symbols[list->count]));
    list->count++;
    return true;
}

static bool stock_list_remove_symbol(stock_list_t *list, const char *symbol)
{
    int idx = stock_list_find_symbol(list, symbol);
    if (idx < 0) {
        return false;
    }

    for (int i = idx; i < list->count - 1; i++) {
        memcpy(list->symbols[i], list->symbols[i + 1], sizeof(list->symbols[i]));
    }
    if (list->count > 0) {
        list->count--;
        memset(list->symbols[list->count], 0, sizeof(list->symbols[list->count]));
    }
    return true;
}

static stock_meta_cache_t *find_stock_meta(const char *symbol)
{
    if (!symbol) {
        return NULL;
    }
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (strcmp(s_stock_meta_cache[i].symbol, symbol) == 0) {
            return &s_stock_meta_cache[i];
        }
    }
    return NULL;
}

static void cache_stock_meta(const char *symbol, const char *name,
                             const char *abbr, const char *industry)
{
    if (!symbol || symbol[0] == '\0') {
        return;
    }

    stock_meta_cache_t *slot = find_stock_meta(symbol);
    if (!slot) {
        for (int i = 0; i < MAX_STOCK_COUNT; i++) {
            if (s_stock_meta_cache[i].symbol[0] == '\0') {
                slot = &s_stock_meta_cache[i];
                break;
            }
        }
    }
    if (!slot) {
        slot = &s_stock_meta_cache[0];
    }

    strlcpy(slot->symbol, symbol, sizeof(slot->symbol));
    strlcpy(slot->name, name ? name : "", sizeof(slot->name));
    strlcpy(slot->abbr, abbr ? abbr : "", sizeof(slot->abbr));
    strlcpy(slot->industry, industry ? industry : "", sizeof(slot->industry));
}

static void clear_stock_meta(const char *symbol)
{
    stock_meta_cache_t *slot = find_stock_meta(symbol);
    if (slot) {
        memset(slot, 0, sizeof(*slot));
    }
}

static void clear_cached_quote(const char *symbol)
{
    if (!symbol || symbol[0] == '\0') {
        return;
    }

    portENTER_CRITICAL(&s_quote_cache_mux);
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (s_stock_quote_cache[i].has_quote &&
            strncmp(s_stock_quote_cache[i].symbol, symbol,
                    sizeof(s_stock_quote_cache[i].symbol)) == 0) {
            memset(&s_stock_quote_cache[i], 0, sizeof(s_stock_quote_cache[i]));
            break;
        }
    }
    portEXIT_CRITICAL(&s_quote_cache_mux);
}

static bool get_cached_quote(const char *symbol, stock_quote_cache_t *out)
{
    if (!symbol || !out || symbol[0] == '\0') {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_quote_cache_mux);
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (s_stock_quote_cache[i].has_quote &&
            strncmp(s_stock_quote_cache[i].symbol, symbol,
                    sizeof(s_stock_quote_cache[i].symbol)) == 0) {
            *out = s_stock_quote_cache[i];
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_quote_cache_mux);
    return found;
}

static const char *cached_limit_status_str(uint8_t status)
{
    if (status == QUOTE_LIMIT_UP) return "up";
    if (status == QUOTE_LIMIT_DOWN) return "down";
    if (status == QUOTE_LIMIT_NONE) return "none";
    return "unknown";
}

static void add_quote_to_stock_item(cJSON *item, const stock_quote_t *quote);

static void add_cached_quote_to_stock_item(cJSON *item, const stock_quote_cache_t *q)
{
    if (!item || !q || !q->has_quote) {
        add_quote_to_stock_item(item, NULL);
        return;
    }

    cJSON *quote_obj = cJSON_CreateObject();
    if (!quote_obj) {
        return;
    }

    cJSON_AddBoolToObject(quote_obj, "available", q->available);
    cJSON_AddStringToObject(quote_obj, "limit_status", cached_limit_status_str(q->limit_status));
    cJSON_AddBoolToObject(quote_obj, "is_market_closed", q->is_market_closed);
    if (q->available) {
        cJSON_AddNumberToObject(quote_obj, "price", q->price);
        cJSON_AddNumberToObject(quote_obj, "change_percent", q->change_percent);
    }

    cJSON_AddItemToObject(item, "quote", quote_obj);
}

static void ensure_stock_meta_cached(const char *symbol)
{
    if (!symbol || symbol[0] == '\0') {
        return;
    }

    stock_meta_cache_t *meta = find_stock_meta(symbol);
    if (meta && (meta->name[0] != '\0' || meta->abbr[0] != '\0' || meta->industry[0] != '\0')) {
        return;
    }

    stock_meta_t stored = {0};
    if (storage_stock_meta_load(symbol, &stored) == ESP_OK) {
        cache_stock_meta(symbol, stored.name, stored.abbr, stored.industry);
    }
}

static bool quote_is_available(const stock_quote_t *quote)
{
    return quote && (quote->is_valid || quote->is_market_closed);
}

static float quote_display_price(const stock_quote_t *quote)
{
    if (!quote) {
        return 0.0f;
    }
    return quote->is_market_closed ? quote->yesterday_close : quote->current_price;
}

static const char *quote_limit_status(const stock_quote_t *quote)
{
    if (!quote_is_available(quote)) {
        return "unknown";
    }
    if (!quote->has_limit_bounds) {
        return "unknown";
    }

    float price = quote_display_price(quote);
    if (float_nearly_equal(price, quote->limit_up_price)) {
        return "up";
    }
    if (float_nearly_equal(price, quote->limit_down_price)) {
        return "down";
    }
    return "none";
}

static void add_quote_to_stock_item(cJSON *item, const stock_quote_t *quote)
{
    if (!item) {
        return;
    }

    cJSON *quote_obj = cJSON_CreateObject();
    if (!quote_obj) {
        return;
    }

    bool available = quote_is_available(quote);
    cJSON_AddBoolToObject(quote_obj, "available", available);
    cJSON_AddStringToObject(quote_obj, "limit_status", quote_limit_status(quote));
    cJSON_AddBoolToObject(quote_obj, "is_market_closed", quote ? quote->is_market_closed : false);
    if (available) {
        cJSON_AddNumberToObject(quote_obj, "price", quote_display_price(quote));
        cJSON_AddNumberToObject(quote_obj, "change_percent", quote->change_percent);
    }

    cJSON_AddItemToObject(item, "quote", quote_obj);
}

static void add_alert_config_to_stock_item(cJSON *item, const stock_alert_config_t *cfg)
{
    if (!item || !cfg) {
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return;
    }
    cJSON_AddBoolToObject(obj, "enabled", cfg->enabled);
    cJSON_AddNumberToObject(obj, "up_threshold_pct", roundf(cfg->up_threshold_pct * 100.0f) / 100.0f);
    cJSON_AddNumberToObject(obj, "down_threshold_pct", roundf(cfg->down_threshold_pct * 100.0f) / 100.0f);
    cJSON_AddStringToObject(obj, "ai_prompt", cfg->ai_prompt);
    cJSON_AddItemToObject(item, "alert_config", obj);
}

static bool is_alert_threshold_valid(double value)
{
    return isfinite(value) && value >= 0.0 && value <= ALERT_THRESHOLD_MAX;
}

static esp_err_t parse_alert_config_from_json(cJSON *alert_obj, stock_alert_config_t *cfg, const char **error_code)
{
    if (!cJSON_IsObject(alert_obj) || !cfg) {
        if (error_code) {
            *error_code = ERR_INVALID_FORMAT;
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *enabled = cJSON_GetObjectItem(alert_obj, "enabled");
    cJSON *up = cJSON_GetObjectItem(alert_obj, "up_threshold_pct");
    cJSON *down = cJSON_GetObjectItem(alert_obj, "down_threshold_pct");
    cJSON *prompt = cJSON_GetObjectItem(alert_obj, "ai_prompt");
    if (!cJSON_IsBool(enabled) || !cJSON_IsNumber(up) || !cJSON_IsNumber(down) || !cJSON_IsString(prompt)) {
        if (error_code) {
            *error_code = ERR_INVALID_FORMAT;
        }
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_alert_threshold_valid(up->valuedouble) || !is_alert_threshold_valid(down->valuedouble)) {
        if (error_code) {
            *error_code = ERR_INVALID_THRESHOLD;
        }
        return ESP_ERR_INVALID_ARG;
    }

    size_t prompt_len = strlen(prompt->valuestring);
    if (prompt_len > ALERT_PROMPT_MAX_BYTES) {
        if (error_code) {
            *error_code = ERR_PROMPT_TOO_LONG;
        }
        return ESP_ERR_INVALID_SIZE;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = cJSON_IsTrue(enabled);
    cfg->up_threshold_pct = roundf((float)up->valuedouble * 100.0f) / 100.0f;
    cfg->down_threshold_pct = roundf((float)down->valuedouble * 100.0f) / 100.0f;
    strlcpy(cfg->ai_prompt, prompt->valuestring, sizeof(cfg->ai_prompt));
    return ESP_OK;
}

static esp_err_t portal_stocks_get_handler(httpd_req_t *req)
{
    /* Heap-allocate all large structs to stay within HTTPD 4KB stack */
    stock_list_t *list = calloc(1, sizeof(stock_list_t));
    stock_alert_config_t *alert_cfg = calloc(1, sizeof(stock_alert_config_t));
    stock_quote_cache_t *cached_quote = calloc(1, sizeof(stock_quote_cache_t));
    if (!list || !alert_cfg || !cached_quote) {
        free(list); free(alert_cfg); free(cached_quote);
        return send_json_error(req, 500, "no_memory");
    }

    esp_err_t ret = storage_stocks_load(list);
    if (ret != ESP_OK) {
        free(list); free(alert_cfg); free(cached_quote);
        return send_json_error(req, 500, "load_failed");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !items) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        free(list); free(alert_cfg); free(cached_quote);
        return send_json_error(req, 500, "no_memory");
    }

    cJSON_AddNumberToObject(root, "count", list->count);
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddItemToObject(root, "items", items);

    for (int i = 0; i < list->count; i++) {
        ensure_stock_meta_cached(list->symbols[i]);
        const char *name = "";
        const char *abbr = "";
        const char *industry = "";
        stock_meta_cache_t *meta = find_stock_meta(list->symbols[i]);
        if (meta) {
            name = meta->name;
            abbr = meta->abbr;
            industry = meta->industry;
        }

        cJSON *item = cJSON_CreateObject();
        if (!item) {
            free(list); free(alert_cfg); free(cached_quote);
            cJSON_Delete(root);
            return send_json_error(req, 500, "no_memory");
        }
        cJSON_AddStringToObject(item, "symbol", list->symbols[i]);
        cJSON_AddStringToObject(item, "name", name ? name : "");
        cJSON_AddStringToObject(item, "abbr", abbr ? abbr : "");
        cJSON_AddStringToObject(item, "industry", industry ? industry : "");
        memset(alert_cfg, 0, sizeof(stock_alert_config_t));
        if (storage_stock_alert_config_load(list->symbols[i], alert_cfg) != ESP_OK) {
            free(list); free(alert_cfg); free(cached_quote);
            cJSON_Delete(root);
            return send_json_error(req, 500, "load_failed");
        }
        add_alert_config_to_stock_item(item, alert_cfg);
        if (get_cached_quote(list->symbols[i], cached_quote)) {
            add_cached_quote_to_stock_item(item, cached_quote);
        } else {
            add_quote_to_stock_item(item, NULL);
        }
        cJSON_AddItemToArray(items, item);
    }

    free(list); free(alert_cfg); free(cached_quote);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return send_json_error(req, 500, "encode_failed");
    }

    ret = send_json_response(req, 200, json);
    free(json);
    return ret;
}

static esp_err_t portal_stocks_add_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    if (!cJSON_IsString(sym) || !is_symbol_format_valid(sym->valuestring)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    char symbol[8] = {0};
    strlcpy(symbol, sym->valuestring, sizeof(symbol));
    cJSON_Delete(root);

    /* Heap-allocate all >128B structs to stay within HTTPD 4KB stack */
    stock_list_t *list = calloc(1, sizeof(stock_list_t));
    stock_symbol_info_t *info = calloc(1, sizeof(stock_symbol_info_t));
    stock_alert_config_t *alert_cfg = calloc(1, sizeof(stock_alert_config_t));
    stock_quote_t *quote = calloc(1, sizeof(stock_quote_t));
    if (!list || !info || !alert_cfg || !quote) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 500, "no_memory");
    }

    if (storage_stocks_load(list) != ESP_OK) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 500, "load_failed");
    }

    if (stock_list_find_symbol(list, symbol) >= 0) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 409, ERR_DUPLICATE_SYMBOL);
    }
    if (list->count >= MAX_STOCK_COUNT) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 409, ERR_LIMIT_EXCEEDED);
    }

    if (!is_sta_connected()) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 502, ERR_VALIDATE_FAILED);
    }

    if (twse_client_validate_symbol_with_quote(symbol, info, quote) != ESP_OK) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 502, ERR_VALIDATE_FAILED);
    }
    if (!info->exists || strcmp(info->market, "tse") != 0) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 404, ERR_NOT_FOUND_OR_NOT_TSE);
    }

    if (!stock_list_add_symbol(list, symbol) || storage_stocks_save(list) != ESP_OK) {
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 500, "save_failed");
    }

    if (storage_stock_meta_save(symbol, info->name, info->short_name, info->industry) != ESP_OK) {
        stock_list_remove_symbol(list, symbol);
        storage_stocks_save(list);
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 500, "save_failed");
    }

    if (storage_stock_alert_config_save(symbol, alert_cfg) != ESP_OK) {
        stock_list_remove_symbol(list, symbol);
        storage_stocks_save(list);
        storage_stock_meta_remove(symbol);
        free(list); free(info); free(alert_cfg); free(quote);
        return send_json_error(req, 500, "save_failed");
    }

    scheduler_service_reload_stock_list();
    cache_stock_meta(symbol, info->name, info->short_name, info->industry);
    stock_admin_service_cache_quote(quote);
    if (s_stock_list_changed_cb) {
        s_stock_list_changed_cb(list->count);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON *item = cJSON_CreateObject();
    if (!resp || !item) {
        free(list); free(info); free(alert_cfg); free(quote);
        cJSON_Delete(resp);
        cJSON_Delete(item);
        return send_json_error(req, 500, "no_memory");
    }

    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddItemToObject(resp, "item", item);
    cJSON_AddStringToObject(item, "symbol", symbol);
    cJSON_AddStringToObject(item, "name", info->name);
    cJSON_AddStringToObject(item, "abbr", info->short_name);
    cJSON_AddStringToObject(item, "industry", info->industry);
    cJSON_AddStringToObject(item, "market", "tse");
    add_alert_config_to_stock_item(item, alert_cfg);
    add_quote_to_stock_item(item, quote);

    free(list); free(info); free(alert_cfg); free(quote);

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (!resp_json) {
        return send_json_error(req, 500, "encode_failed");
    }

    esp_err_t send_ret = send_json_response(req, 200, resp_json);
    free(resp_json);
    return send_ret;
}

static esp_err_t portal_stocks_update_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    cJSON *alert_obj = cJSON_GetObjectItem(root, "alert_config");
    cJSON *clear_alert = cJSON_GetObjectItem(root, "clear_alert");
    if (!cJSON_IsString(sym) || !is_symbol_format_valid(sym->valuestring)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    bool should_clear_alert = cJSON_IsTrue(clear_alert);
    if (clear_alert && !cJSON_IsBool(clear_alert)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    char symbol[8] = {0};
    strlcpy(symbol, sym->valuestring, sizeof(symbol));

    /* Heap-allocate all large structs to stay within HTTPD 4KB stack */
    stock_list_t *list = calloc(1, sizeof(stock_list_t));
    stock_alert_config_t *alert_cfg = calloc(1, sizeof(stock_alert_config_t));
    stock_quote_cache_t *cached_quote = calloc(1, sizeof(stock_quote_cache_t));
    if (!list || !alert_cfg || !cached_quote) {
        free(list); free(alert_cfg); free(cached_quote);
        cJSON_Delete(root);
        return send_json_error(req, 500, "no_memory");
    }

    if (storage_stocks_load(list) != ESP_OK) {
        free(list); free(alert_cfg); free(cached_quote);
        cJSON_Delete(root);
        return send_json_error(req, 500, "load_failed");
    }
    if (stock_list_find_symbol(list, symbol) < 0) {
        free(list); free(alert_cfg); free(cached_quote);
        cJSON_Delete(root);
        return send_json_error(req, 404, ERR_NOT_FOUND);
    }
    free(list);

    if (should_clear_alert) {
        if (storage_stock_alert_config_remove(symbol) != ESP_OK) {
            free(alert_cfg); free(cached_quote);
            cJSON_Delete(root);
            return send_json_error(req, 500, "save_failed");
        }
    } else {
        const char *parse_error = NULL;
        esp_err_t parse_ret = parse_alert_config_from_json(alert_obj, alert_cfg, &parse_error);
        if (parse_ret != ESP_OK) {
            free(alert_cfg); free(cached_quote);
            cJSON_Delete(root);
            return send_json_error(req, 400, parse_error);
        }
        if (storage_stock_alert_config_save(symbol, alert_cfg) != ESP_OK) {
            free(alert_cfg); free(cached_quote);
            cJSON_Delete(root);
            return send_json_error(req, 500, "save_failed");
        }
    }
    cJSON_Delete(root);

    if (storage_stock_alert_config_load(symbol, alert_cfg) != ESP_OK) {
        free(alert_cfg); free(cached_quote);
        return send_json_error(req, 500, "load_failed");
    }

    ensure_stock_meta_cached(symbol);
    const char *name = "";
    const char *abbr = "";
    const char *industry = "";
    stock_meta_cache_t *meta = find_stock_meta(symbol);
    if (meta) {
        name = meta->name;
        abbr = meta->abbr;
        industry = meta->industry;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON *item = cJSON_CreateObject();
    if (!resp || !item) {
        free(alert_cfg); free(cached_quote);
        cJSON_Delete(resp);
        cJSON_Delete(item);
        return send_json_error(req, 500, "no_memory");
    }

    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddItemToObject(resp, "item", item);
    cJSON_AddStringToObject(item, "symbol", symbol);
    cJSON_AddStringToObject(item, "name", name);
    cJSON_AddStringToObject(item, "abbr", abbr);
    cJSON_AddStringToObject(item, "industry", industry);
    add_alert_config_to_stock_item(item, alert_cfg);
    if (get_cached_quote(symbol, cached_quote)) {
        add_cached_quote_to_stock_item(item, cached_quote);
    } else {
        add_quote_to_stock_item(item, NULL);
    }

    free(alert_cfg); free(cached_quote);

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (!resp_json) {
        return send_json_error(req, 500, "encode_failed");
    }

    esp_err_t send_ret = send_json_response(req, 200, resp_json);
    free(resp_json);
    return send_ret;
}

static esp_err_t portal_stocks_remove_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_error(req, 400, ERR_NOT_FOUND);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return send_json_error(req, 400, ERR_NOT_FOUND);
    }

    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    if (!cJSON_IsString(sym)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_NOT_FOUND);
    }

    char symbol[8] = {0};
    strlcpy(symbol, sym->valuestring, sizeof(symbol));
    cJSON_Delete(root);

    stock_list_t *list = calloc(1, sizeof(stock_list_t));
    if (!list) {
        return send_json_error(req, 500, "no_memory");
    }
    if (storage_stocks_load(list) != ESP_OK) {
        free(list);
        return send_json_error(req, 500, "load_failed");
    }

    if (!stock_list_remove_symbol(list, symbol)) {
        free(list);
        return send_json_error(req, 404, ERR_NOT_FOUND);
    }

    if (storage_stocks_save(list) != ESP_OK) {
        free(list);
        return send_json_error(req, 500, "save_failed");
    }
    int final_count = list->count;
    free(list);
    if (storage_stock_meta_remove(symbol) != ESP_OK) {
        return send_json_error(req, 500, "save_failed");
    }
    if (storage_stock_alert_config_remove(symbol) != ESP_OK) {
        return send_json_error(req, 500, "save_failed");
    }

    scheduler_service_reload_stock_list();
    clear_stock_meta(symbol);
    clear_cached_quote(symbol);
    if (s_stock_list_changed_cb) {
        s_stock_list_changed_cb(final_count);
    }

    return send_json_response(req, 200, "{\"ok\":true}");
}

static esp_err_t portal_saved_aps_get_handler(httpd_req_t *req)
{
    char buf[256];
    size_t pos = 0;
    uint8_t count = storage_wifi_ap_count();
    bool sta_connected = wifi_manager_is_connected();
    const char *connected_ssid = wifi_manager_get_connected_ssid();
    bool has_connected_ssid = (sta_connected && connected_ssid && connected_ssid[0] != '\0');

    buf[pos++] = '[';
    for (uint8_t i = 0; i < count && pos < sizeof(buf) - 2; i++) {
        char ssid[WIFI_SSID_MAX_LEN] = {0};
        char pass[WIFI_PASS_MAX_LEN] = {0};
        if (storage_wifi_load_ap(i, ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
            continue;
        }

        char esc[68] = {0};
        json_escape(esc, sizeof(esc), ssid);
        bool is_connected = has_connected_ssid && (strcmp(ssid, connected_ssid) == 0);
        int n = snprintf(buf + pos, sizeof(buf) - pos,
                         "%s{\"ssid\":\"%s\",\"connected\":%s}",
                         (pos > 1) ? "," : "",
                         esc,
                         is_connected ? "true" : "false");
        if (n <= 0 || (size_t)n >= sizeof(buf) - pos) {
            break;
        }
        pos += (size_t)n;
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, (ssize_t)pos);
}

static esp_err_t portal_saved_aps_remove_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    char target_ssid[WIFI_SSID_MAX_LEN] = {0};
    bool has_ssid = get_form_value(body, "ssid", target_ssid, sizeof(target_ssid));
    free(body);

    if (!has_ssid || target_ssid[0] == '\0') {
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    uint8_t count = storage_wifi_ap_count();
    for (uint8_t i = 0; i < count; i++) {
        char ssid[WIFI_SSID_MAX_LEN] = {0};
        char pass[WIFI_PASS_MAX_LEN] = {0};
        if (storage_wifi_load_ap(i, ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
            continue;
        }

        if (strcmp(ssid, target_ssid) == 0) {
            if (storage_wifi_remove_ap(i) != ESP_OK) {
                return send_json_error(req, 500, "remove_failed");
            }
            return send_json_response(req, 200, "{\"ok\":true}");
        }
    }

    return send_json_error(req, 404, ERR_NOT_FOUND);
}

void stock_admin_service_set_stock_list_changed_callback(stock_list_changed_cb_t cb)
{
    s_stock_list_changed_cb = cb;
}

void stock_admin_service_cache_quote(const stock_quote_t *quote)
{
    if (!quote || quote->symbol[0] == '\0') {
        return;
    }

    portENTER_CRITICAL(&s_quote_cache_mux);

    /* Find existing slot or first empty */
    stock_quote_cache_t *slot = NULL;
    stock_quote_cache_t *empty = NULL;
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (s_stock_quote_cache[i].has_quote &&
            strncmp(s_stock_quote_cache[i].symbol, quote->symbol,
                    sizeof(s_stock_quote_cache[i].symbol)) == 0) {
            slot = &s_stock_quote_cache[i];
            break;
        }
        if (!s_stock_quote_cache[i].has_quote && !empty) {
            empty = &s_stock_quote_cache[i];
        }
    }
    if (!slot) {
        slot = empty ? empty : &s_stock_quote_cache[0];
    }

    slot->has_quote = true;
    strlcpy(slot->symbol, quote->symbol, sizeof(slot->symbol));
    slot->available = quote_is_available(quote);
    slot->is_market_closed = quote->is_market_closed;
    slot->price = quote_display_price(quote);
    slot->change_percent = quote->change_percent;
    if (slot->available) {
        const char *limit = quote_limit_status(quote);
        slot->limit_status = (strcmp(limit, "up") == 0) ? QUOTE_LIMIT_UP
                           : (strcmp(limit, "down") == 0) ? QUOTE_LIMIT_DOWN
                           : (strcmp(limit, "none") == 0) ? QUOTE_LIMIT_NONE
                           : QUOTE_LIMIT_UNKNOWN;
    } else {
        slot->limit_status = QUOTE_LIMIT_UNKNOWN;
    }

    portEXIT_CRITICAL(&s_quote_cache_mux);
}

esp_err_t stock_admin_service_register_handlers(httpd_handle_t httpd)
{
    if (!httpd) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t stocks_uri = {
        .uri = "/api/stocks",
        .method = HTTP_GET,
        .handler = portal_stocks_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_add_uri = {
        .uri = "/api/stocks/add",
        .method = HTTP_POST,
        .handler = portal_stocks_add_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_remove_uri = {
        .uri = "/api/stocks/remove",
        .method = HTTP_POST,
        .handler = portal_stocks_remove_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_update_uri = {
        .uri = "/api/stocks/update",
        .method = HTTP_POST,
        .handler = portal_stocks_update_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t saved_aps_get_uri = {
        .uri = "/api/saved_aps",
        .method = HTTP_GET,
        .handler = portal_saved_aps_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t saved_aps_remove_uri = {
        .uri = "/api/saved_aps/remove",
        .method = HTTP_POST,
        .handler = portal_saved_aps_remove_post_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(httpd, &stocks_uri);
    httpd_register_uri_handler(httpd, &stocks_add_uri);
    httpd_register_uri_handler(httpd, &stocks_remove_uri);
    httpd_register_uri_handler(httpd, &stocks_update_uri);
    httpd_register_uri_handler(httpd, &saved_aps_get_uri);
    httpd_register_uri_handler(httpd, &saved_aps_remove_uri);

    ESP_LOGI(TAG, "stock admin handlers registered");
    return ESP_OK;
}
