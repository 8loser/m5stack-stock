#include "device_server.h"
#include "storage.h"
#include "scheduler.h"
#include "twse_client.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "device_srv";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           5
#define PORTAL_BODY_MAX_LEN 4096
#define PORTAL_SCAN_MAX_APS 20
#define PORTAL_JSON_MAX_LEN 2048

#define ERR_INVALID_FORMAT      "invalid_format"
#define ERR_DUPLICATE_SYMBOL    "duplicate_symbol"
#define ERR_LIMIT_EXCEEDED      "limit_exceeded"
#define ERR_NOT_FOUND_OR_NOT_TSE "not_found_or_not_tse"
#define ERR_VALIDATE_FAILED     "validate_failed"
#define ERR_NOT_FOUND           "not_found"

enum {
    AI_PROVIDER_GEMINI = 0,
    AI_PROVIDER_CLAUDE = 1,
    AI_PROVIDER_OPENAI = 2,
};

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t       s_state            = WIFI_STATE_DISCONNECTED;
static char               s_ip_str[20]       = "0.0.0.0";
static char               s_connected_ssid[33] = "";
static int                s_retry_count      = 0;
static wifi_state_cb_t    s_callback         = NULL;
static bool               s_initialized      = false;

static esp_netif_t       *s_ap_netif         = NULL;
static httpd_handle_t     s_httpd            = NULL;
static bool               s_portal_active    = false;
static bool               s_connecting_busy  = false;

static char s_portal_ap_ssid[33]     = WIFI_PORTAL_AP_SSID;
static char s_portal_ap_password[65] = WIFI_PORTAL_AP_PASSWORD;
static const char *s_portal_url      = WIFI_PORTAL_URL;

typedef struct {
    char symbol[8];
    char name[64];
    char abbr[32];
} stock_meta_cache_t;

static stock_meta_cache_t s_stock_meta_cache[MAX_STOCK_COUNT];

typedef struct {
    char ssid[33];
    char password[65];
} wifi_connect_req_t;

static void notify_state(wifi_state_t new_state)
{
    s_state = new_state;
    if (s_callback) s_callback(new_state, s_ip_str);
}

static void url_decode(char *dst, size_t dst_len, const char *src)
{
    size_t di = 0;

    if (!dst || dst_len == 0) return;
    dst[0] = '\0';

    for (size_t i = 0; src && src[i] != '\0' && di + 1 < dst_len; i++) {
        if (src[i] == '+') {
            dst[di++] = ' ';
            continue;
        }

        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            char hex[3] = { src[i + 1], src[i + 2], '\0' };
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
    if (!body || !key || !out || out_len == 0) return false;

    size_t key_len = strlen(key);
    const char *p = body;

    while (p && *p) {
        if ((p == body || p[-1] == '&') && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            const char *end = strchr(p, '&');
            size_t raw_len = end ? (size_t)(end - p) : strlen(p);

            char tmp[PORTAL_BODY_MAX_LEN + 1] = {0};
            if (raw_len > PORTAL_BODY_MAX_LEN) raw_len = PORTAL_BODY_MAX_LEN;
            memcpy(tmp, p, raw_len);
            tmp[raw_len] = '\0';

            url_decode(out, out_len, tmp);
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }

    return false;
}

static void wifi_connect_task(void *arg)
{
    wifi_connect_req_t *req = (wifi_connect_req_t *)arg;
    if (!req) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Portal 提交配網，嘗試連線 SSID=%s", req->ssid);
    esp_err_t ret = device_server_connect(req->ssid, req->password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Portal 配網成功");
        if (s_portal_active) {
            esp_err_t stop_ret = device_server_stop_provisioning_portal();
            if (stop_ret != ESP_OK) {
                ESP_LOGW(TAG, "Portal 停止失敗: %s", esp_err_to_name(stop_ret));
            }
        }
    } else {
        ESP_LOGW(TAG, "Portal 配網失敗");
    }

    s_connecting_busy = false;
    free(req);
    vTaskDelete(NULL);
}

static void json_escape(char *out, size_t out_len, const char *in)
{
    size_t di = 0;
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!in) return;

    for (size_t i = 0; in[i] && di + 2 < out_len; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            if (di + 3 >= out_len) break;
            out[di++] = '\\';
            out[di++] = c;
        } else if (c == '\n') {
            if (di + 3 >= out_len) break;
            out[di++] = '\\';
            out[di++] = 'n';
        } else if (c == '\r') {
            if (di + 3 >= out_len) break;
            out[di++] = '\\';
            out[di++] = 'r';
        } else if (c == '\t') {
            if (di + 3 >= out_len) break;
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
    if (!req || !out_body) return ESP_ERR_INVALID_ARG;
    *out_body = NULL;
    if (req->content_len <= 0 || req->content_len > PORTAL_BODY_MAX_LEN) return ESP_ERR_INVALID_ARG;

    char *body = malloc((size_t)req->content_len + 1);
    if (!body) return ESP_ERR_NO_MEM;

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
    if (!symbol || strlen(symbol) != 4) return false;
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)symbol[i])) return false;
    }
    return true;
}

static int stock_list_find_symbol(const stock_list_t *list, const char *symbol)
{
    if (!list || !symbol) return -1;
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i], symbol) == 0) return i;
    }
    return -1;
}

static bool stock_list_add_symbol(stock_list_t *list, const char *symbol)
{
    if (!list || !symbol || list->count >= MAX_STOCK_COUNT) return false;
    strlcpy(list->symbols[list->count], symbol, sizeof(list->symbols[list->count]));
    list->count++;
    return true;
}

static bool stock_list_remove_symbol(stock_list_t *list, const char *symbol)
{
    int idx = stock_list_find_symbol(list, symbol);
    if (idx < 0) return false;

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
    if (!symbol) return NULL;
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        if (strcmp(s_stock_meta_cache[i].symbol, symbol) == 0) {
            return &s_stock_meta_cache[i];
        }
    }
    return NULL;
}

static void cache_stock_meta(const char *symbol, const char *name, const char *abbr)
{
    if (!symbol || symbol[0] == '\0') return;

    stock_meta_cache_t *slot = find_stock_meta(symbol);
    if (!slot) {
        for (int i = 0; i < MAX_STOCK_COUNT; i++) {
            if (s_stock_meta_cache[i].symbol[0] == '\0') {
                slot = &s_stock_meta_cache[i];
                break;
            }
        }
    }
    if (!slot) slot = &s_stock_meta_cache[0];

    strlcpy(slot->symbol, symbol, sizeof(slot->symbol));
    strlcpy(slot->name, name ? name : "", sizeof(slot->name));
    strlcpy(slot->abbr, abbr ? abbr : "", sizeof(slot->abbr));
}

static void clear_stock_meta(const char *symbol)
{
    stock_meta_cache_t *slot = find_stock_meta(symbol);
    if (slot) memset(slot, 0, sizeof(*slot));
}

static esp_err_t portal_scan_get_handler(httpd_req_t *req)
{
    wifi_ap_record_t *ap_records = malloc(PORTAL_SCAN_MAX_APS * sizeof(wifi_ap_record_t));
    if (!ap_records) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ret;
    }

    uint16_t ap_count = PORTAL_SCAN_MAX_APS;
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan get failed");
        return ret;
    }

    /* 每筆 entry 最大約 90 bytes（SSID 32 chars * 2 escape + 固定格式）
       20 * 90 + 括號 + null < 2048 */
    char *buf = malloc(2048);
    if (!buf) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    char esc[68];   /* 32 chars * 2 worst-case escape + null */
    size_t pos = 0;
    buf[pos++] = '[';
    for (uint16_t i = 0; i < ap_count && pos < 2040; i++) {
        json_escape(esc, sizeof(esc), (const char *)ap_records[i].ssid);
        int n = snprintf(buf + pos, 2048 - pos,
                         "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                         i > 0 ? "," : "", esc, (int)ap_records[i].rssi);
        if (n > 0) pos += (size_t)n;
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    free(ap_records);
    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_send(req, buf, (ssize_t)pos);
    free(buf);
    return ret;
}

static esp_err_t portal_ai_get_handler(httpd_req_t *req)
{
    char gemini_key[128] = {0};
    char claude_key[128] = {0};
    char openai_key[128] = {0};
    char prompt_template[1024] = {0};
    char e_gemini[300] = {0};
    char e_claude[300] = {0};
    char e_openai[300] = {0};
    char e_prompt[2200] = {0};

    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key, sizeof(gemini_key));
    if (gemini_key[0] == '\0') {
        storage_ai_load_key(gemini_key, sizeof(gemini_key));
    }
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key, sizeof(claude_key));
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key, sizeof(openai_key));
    storage_ai_load_prompt_template(prompt_template, sizeof(prompt_template));

    json_escape(e_gemini, sizeof(e_gemini), gemini_key);
    json_escape(e_claude, sizeof(e_claude), claude_key);
    json_escape(e_openai, sizeof(e_openai), openai_key);
    json_escape(e_prompt, sizeof(e_prompt), prompt_template);

    char *json = malloc(3200);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    int n = snprintf(json, 3200,
                     "{\"gemini_key\":\"%s\",\"claude_key\":\"%s\","
                     "\"openai_key\":\"%s\",\"prompt_template\":\"%s\"}",
                     e_gemini, e_claude, e_openai, e_prompt);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, n);
    free(json);
    return ret;
}

static esp_err_t portal_ai_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid request");
        return ESP_FAIL;
    }

    char gemini_key[128] = {0};
    char claude_key[128] = {0};
    char openai_key[128] = {0};
    char prompt_template[1024] = {0};

    get_form_value(body, "gemini_key", gemini_key, sizeof(gemini_key));
    get_form_value(body, "claude_key", claude_key, sizeof(claude_key));
    get_form_value(body, "openai_key", openai_key, sizeof(openai_key));
    get_form_value(body, "prompt_template", prompt_template, sizeof(prompt_template));
    free(body);

    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key);
    storage_ai_save_prompt_template(prompt_template);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req,
        "<html><body><h3>AI 設定已儲存</h3>"
        "<p>可返回上一頁繼續調整。</p><a href='/'>Back</a></body></html>");
}

static esp_err_t portal_stocks_get_handler(httpd_req_t *req)
{
    stock_list_t list = {0};
    esp_err_t ret = storage_stocks_load(&list);
    if (ret != ESP_OK) {
        return send_json_error(req, 500, "load_failed");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !items) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        return send_json_error(req, 500, "no_memory");
    }

    cJSON_AddNumberToObject(root, "count", list.count);
    cJSON_AddItemToObject(root, "items", items);

    for (int i = 0; i < list.count; i++) {
        const char *name = "";
        const char *abbr = "";
        stock_meta_cache_t *meta = find_stock_meta(list.symbols[i]);
        if (meta) {
            name = meta->name;
            abbr = meta->abbr;
        }

        if ((!name || name[0] == '\0') && is_sta_connected()) {
            stock_symbol_info_t info = {0};
            if (twse_client_validate_symbol(list.symbols[i], &info) == ESP_OK && info.exists) {
                cache_stock_meta(list.symbols[i], info.name, info.short_name);
                meta = find_stock_meta(list.symbols[i]);
                if (meta) {
                    name = meta->name;
                    abbr = meta->abbr;
                }
            }
        }

        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            return send_json_error(req, 500, "no_memory");
        }
        cJSON_AddStringToObject(item, "symbol", list.symbols[i]);
        cJSON_AddStringToObject(item, "name", name ? name : "");
        cJSON_AddStringToObject(item, "abbr", abbr ? abbr : "");
        cJSON_AddItemToArray(items, item);
    }

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
    if (!root) return send_json_error(req, 400, ERR_INVALID_FORMAT);

    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    if (!cJSON_IsString(sym) || !is_symbol_format_valid(sym->valuestring)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_INVALID_FORMAT);
    }

    char symbol[8] = {0};
    strlcpy(symbol, sym->valuestring, sizeof(symbol));
    cJSON_Delete(root);

    stock_list_t list = {0};
    if (storage_stocks_load(&list) != ESP_OK) {
        return send_json_error(req, 500, "load_failed");
    }

    if (stock_list_find_symbol(&list, symbol) >= 0) {
        return send_json_error(req, 409, ERR_DUPLICATE_SYMBOL);
    }
    if (list.count >= MAX_STOCK_COUNT) {
        return send_json_error(req, 409, ERR_LIMIT_EXCEEDED);
    }

    if (!is_sta_connected()) {
        return send_json_error(req, 502, ERR_VALIDATE_FAILED);
    }

    stock_symbol_info_t info = {0};
    if (twse_client_validate_symbol(symbol, &info) != ESP_OK) {
        return send_json_error(req, 502, ERR_VALIDATE_FAILED);
    }
    if (!info.exists || strcmp(info.market, "tse") != 0) {
        return send_json_error(req, 404, ERR_NOT_FOUND_OR_NOT_TSE);
    }

    if (!stock_list_add_symbol(&list, symbol) ||
        storage_stocks_save(&list) != ESP_OK) {
        return send_json_error(req, 500, "save_failed");
    }

    scheduler_reload_stock_list();
    cache_stock_meta(symbol, info.name, info.short_name);

    char esc_name[160] = {0};
    char esc_abbr[96] = {0};
    json_escape(esc_name, sizeof(esc_name), info.name);
    json_escape(esc_abbr, sizeof(esc_abbr), info.short_name);

    char resp[PORTAL_JSON_MAX_LEN];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"item\":{\"symbol\":\"%s\",\"name\":\"%s\",\"abbr\":\"%s\",\"market\":\"tse\"}}",
             symbol, esc_name, esc_abbr);
    return send_json_response(req, 200, resp);
}

static esp_err_t portal_stocks_remove_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_error(req, 400, ERR_NOT_FOUND);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return send_json_error(req, 400, ERR_NOT_FOUND);

    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    if (!cJSON_IsString(sym)) {
        cJSON_Delete(root);
        return send_json_error(req, 400, ERR_NOT_FOUND);
    }

    char symbol[8] = {0};
    strlcpy(symbol, sym->valuestring, sizeof(symbol));
    cJSON_Delete(root);

    stock_list_t list = {0};
    if (storage_stocks_load(&list) != ESP_OK) {
        return send_json_error(req, 500, "load_failed");
    }

    if (!stock_list_remove_symbol(&list, symbol)) {
        return send_json_error(req, 404, ERR_NOT_FOUND);
    }

    if (storage_stocks_save(&list) != ESP_OK) {
        return send_json_error(req, 500, "save_failed");
    }

    scheduler_reload_stock_list();
    clear_stock_meta(symbol);
    return send_json_response(req, 200, "{\"ok\":true}");
}

static esp_err_t portal_saved_aps_get_handler(httpd_req_t *req)
{
    char buf[256];
    size_t pos = 0;
    uint8_t count = storage_wifi_ap_count();

    buf[pos++] = '[';
    for (uint8_t i = 0; i < count && pos < sizeof(buf) - 2; i++) {
        char ssid[WIFI_SSID_MAX_LEN] = {0};
        char pass[WIFI_PASS_MAX_LEN] = {0};
        if (storage_wifi_load_ap(i, ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
            continue;
        }

        char esc[68] = {0};
        json_escape(esc, sizeof(esc), ssid);
        int n = snprintf(buf + pos, sizeof(buf) - pos,
                         "%s{\"ssid\":\"%s\"}", (pos > 1) ? "," : "", esc);
        if (n <= 0 || (size_t)n >= sizeof(buf) - pos) break;
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

static esp_err_t portal_index_get_handler(httpd_req_t *req)
{
    static const char *html =
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Core2 Portal</title>"
        "<style>body{font-family:sans-serif;padding:14px;max-width:900px;margin:auto;}"
        "h2{margin:8px 0 6px;}h3{margin:18px 0 6px;}"
        ".grid{display:grid;grid-template-columns:1fr;gap:12px;}"
        "@media(min-width:860px){.grid{grid-template-columns:1fr 1fr;}}"
        ".card{border:1px solid #ddd;border-radius:10px;padding:12px;}"
        "label{display:block;margin-top:8px;font-size:14px;color:#333;}"
        "select,input,textarea,button{width:100%;padding:10px;margin-top:6px;box-sizing:border-box;}"
        "textarea{min-height:180px;resize:vertical;}"
        "button{background:#0b7;color:#fff;border:0;border-radius:6px;font-weight:600;}"
        ".hint{font-size:12px;color:#666;margin-top:4px;}"
        ".ok{color:#0a6;font-size:13px;}.err{color:#b00020;font-size:13px;}"
        ".stock-row{display:flex;justify-content:space-between;align-items:center;"
        "padding:8px 0;border-bottom:1px solid #eee;gap:8px;}"
        ".stock-row:last-child{border-bottom:0;}"
        ".stock-symbol{font-weight:700;}.menu{display:flex;gap:8px;margin:8px 0 14px;}"
        ".menu-btn{flex:1;width:auto;background:#e9eef2;color:#233;border:1px solid #c8d2db;"
        "border-radius:8px;padding:10px 8px;font-weight:700;}"
        ".menu-btn.active{background:#0b7;color:#fff;border-color:#0b7;}"
        ".section-card{display:none;}.section-card.active{display:block;}</style>"
        "</head><body><h2>M5Stack Core2 Portal</h2>"
        "<div class='menu'>"
        "<button type='button' id='tab_wifi' class='menu-btn' onclick=\"showTab('wifi')\">WiFi</button>"
        "<button type='button' id='tab_ai' class='menu-btn' onclick=\"showTab('ai')\">AI Provider</button>"
        "<button type='button' id='tab_stocks' class='menu-btn' onclick=\"showTab('stocks')\">Stocks</button>"
        "</div>"
        "<div class='grid'>"
        "<div id='card_wifi' class='card section-card'>"
        "<h3>WiFi Setup</h3>"
        "<form method='post' action='/wifi'>"
        "<label>SSID</label>"
        "<select id='ss' name='ssid' onchange='chk(this)'>"
        "<option value=''>Scanning...</option></select>"
        "<div id='m' style='display:none'>"
        "<label>Manual SSID</label>"
        "<input name='ssid_manual' maxlength='32'></div>"
        "<label>Password</label>"
        "<input name='password' maxlength='64' type='password'>"
        "<button type='submit'>Connect</button>"
        "<div class='hint'>Connect success will switch Core2 back to STA mode.</div>"
        "</form>"
        "<h3>Saved Networks</h3><div id='saved_aps_list'>Loading...</div>"
        "</div>"
        "<div id='card_ai' class='card section-card'>"
        "<h3>AI Settings</h3>"
        "<form method='post' action='/ai'>"
        "<label>Gemini API Key</label><input id='gemini_key' name='gemini_key' maxlength='127'>"
        "<label>Claude API Key</label><input id='claude_key' name='claude_key' maxlength='127'>"
        "<label>OpenAI API Key</label><input id='openai_key' name='openai_key' maxlength='127'>"
        "<div class='hint'>Only providers with non-empty key will be used.</div>"
        "<label>Prompt Template</label>"
        "<textarea id='prompt_template' name='prompt_template' maxlength='1023' "
        "placeholder='例如：請保守評估，只在高信心時給 buy/sell，其他給 hold'></textarea>"
        "<div class='hint'>This prompt will be appended as AI instruction text.</div>"
        "<button type='submit'>Save AI Settings</button>"
        "</form><div id='ai_msg' class='ok'></div></div>"
        "<div id='card_stocks' class='card section-card'>"
        "<h3>Stocks (TWSE only, max 10)</h3>"
        "<label>Symbol (4 digits)</label>"
        "<input id='stock_symbol' maxlength='4' inputmode='numeric' placeholder='2330'>"
        "<button type='button' onclick='addStock()'>Add</button>"
        "<div id='stocks_msg' class='hint'></div>"
        "<div id='stocks_list' class='hint'>Loading...</div>"
        "</div>"
        "</div>"
        "<script>"
        "var s_saved_ssids=[];"
        "function showTab(tab){"
        "var ids=['wifi','ai','stocks'];"
        "ids.forEach(function(x){"
        "var card=document.getElementById('card_'+x);"
        "var btn=document.getElementById('tab_'+x);"
        "if(card) card.className='card section-card'+(x===tab?' active':'');"
        "if(btn) btn.className='menu-btn'+(x===tab?' active':'');"
        "});"
        "if(tab==='stocks'){loadStocks();}"
        "if(tab==='wifi'){loadSavedAps();}"
        "}"
        "fetch('/scan').then(function(r){return r.json();}).then(function(a){"
        "var s=document.getElementById('ss');"
        "s.options.length=0;"
        "s.add(new Option('-- Select AP --',''));"
        "a.forEach(function(x){"
        "var saved=s_saved_ssids.indexOf(x.ssid)>=0;"
        "var o=new Option();"
        "o.textContent=x.ssid+(saved?' [Saved]':'')+'  ('+x.rssi+'dBm)';"
        "o.value=x.ssid;"
        "s.add(o);});"
        "s.add(new Option('Other (manual)','__manual__'));"
        "}).catch(function(){"
        "var s=document.getElementById('ss');"
        "s.options.length=0;"
        "s.add(new Option('Scan failed','__manual__'));"
        "document.getElementById('m').style.display='block';});"
        "fetch('/ai').then(function(r){return r.json();}).then(function(c){"
        "document.getElementById('gemini_key').value=c.gemini_key||'';"
        "document.getElementById('claude_key').value=c.claude_key||'';"
        "document.getElementById('openai_key').value=c.openai_key||'';"
        "document.getElementById('prompt_template').value=c.prompt_template||'';"
        "}).catch(function(){});"
        "function stockErr(code){var m={invalid_format:'Symbol 必須是 4 位數字',"
        "duplicate_symbol:'已在清單中',limit_exceeded:'最多 10 檔',"
        "not_found_or_not_tse:'找不到代號或非 TWSE 上市',validate_failed:'TWSE 驗證失敗，請稍後再試',"
        "not_found:'清單內找不到此代號'};return m[code]||('Error: '+(code||'unknown'));}"
        "function setStocksMsg(msg,ok){var el=document.getElementById('stocks_msg');"
        "el.textContent=msg||'';el.className=ok?'ok':'err';}"
        "function loadStocks(){fetch('/stocks').then(function(r){return r.json();}).then(function(d){"
        "var box=document.getElementById('stocks_list');"
        "if(!d.items||!d.items.length){box.innerHTML='<div class=\"hint\">No stocks configured</div>';return;}"
        "var html='';d.items.forEach(function(it){html+='<div class=\"stock-row\"><div><span class=\"stock-symbol\">'+"
        "it.symbol+'</span><br><span>'+(it.name||'')+'</span></div><button type=\"button\" style=\"width:auto;padding:6px 10px;background:#c33\" "
        "onclick=\"removeStock(\\''+it.symbol+'\\')\">Remove</button></div>';});box.innerHTML=html;"
        "}).catch(function(){document.getElementById('stocks_list').innerHTML='<div class=\"err\">Load failed</div>';});}"
        "function addStock(){var sym=(document.getElementById('stock_symbol').value||'').trim();"
        "fetch('/stocks/add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({symbol:sym})})"
        ".then(function(r){return r.json().catch(function(){return{};}).then(function(b){return{ok:r.ok,body:b};});})"
        ".then(function(x){if(!x.ok||!x.body.ok){setStocksMsg(stockErr(x.body.error),false);return;}"
        "setStocksMsg('新增成功：'+x.body.item.symbol+' '+(x.body.item.name||''),true);"
        "document.getElementById('stock_symbol').value='';loadStocks();})"
        ".catch(function(e){setStocksMsg('Request failed: '+(e&&e.message?e.message:'network'),false);});}"
        "function removeStock(sym){fetch('/stocks/remove',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({symbol:sym})})"
        ".then(function(r){return r.json().catch(function(){return{};}).then(function(b){return{ok:r.ok,body:b};});})"
        ".then(function(x){if(!x.ok||!x.body.ok){setStocksMsg(stockErr(x.body.error),false);return;}"
        "setStocksMsg('已移除：'+sym,true);loadStocks();})"
        ".catch(function(e){setStocksMsg('Request failed: '+(e&&e.message?e.message:'network'),false);});}"
        "function loadSavedAps(){fetch('/saved_aps').then(function(r){return r.json();}).then(function(a){"
        "s_saved_ssids=(a||[]).map(function(x){return x.ssid||'';}).filter(function(x){return x.length>0;});"
        "var box=document.getElementById('saved_aps_list');if(!box)return;"
        "if(!s_saved_ssids.length){box.innerHTML='<div class=\"hint\">No saved networks</div>';return;}"
        "var h='';s_saved_ssids.forEach(function(ssid){"
        "h+='<div class=\"stock-row\"><div><span class=\"stock-symbol\">'+ssid+'</span></div>' +"
        "'<button type=\"button\" style=\"width:auto;padding:6px 10px;background:#c33\" onclick=\"removeSavedAp(\\''+ssid.replace(/'/g,\"\\\\'\")+'\\')\">Remove</button></div>';});"
        "box.innerHTML=h;}).catch(function(){var box=document.getElementById('saved_aps_list');"
        "if(box)box.innerHTML='<div class=\"err\">Load failed</div>';});}"
        "function removeSavedAp(ssid){var b='ssid='+encodeURIComponent(ssid||'');"
        "fetch('/saved_aps/remove',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})"
        ".then(function(r){return r.json().catch(function(){return{};}).then(function(j){return{ok:r.ok,body:j};});})"
        ".then(function(x){if(!x.ok||!x.body.ok){return;}loadSavedAps();})"
        ".catch(function(){});}"
        "function chk(s){"
        "document.getElementById('m').style.display="
        "(s.value=='__manual__')?'block':'none';}"
        "showTab('wifi');"
        "loadSavedAps();"
        "loadStocks();"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t portal_wifi_post_handler(httpd_req_t *req)
{
    if (!req || req->content_len <= 0 || req->content_len > PORTAL_BODY_MAX_LEN) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid request");
        return ESP_FAIL;
    }

    if (s_connecting_busy) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "already connecting");
        return ESP_FAIL;
    }

    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        return ESP_FAIL;
    }

    wifi_connect_req_t *conn_req = calloc(1, sizeof(wifi_connect_req_t));
    if (!conn_req) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    get_form_value(body, "ssid", conn_req->ssid, sizeof(conn_req->ssid));
    get_form_value(body, "password", conn_req->password, sizeof(conn_req->password));

    /* 若使用者選擇 "Other (manual)"，改從 ssid_manual 欄位取得 SSID */
    if (conn_req->ssid[0] == '\0' || strcmp(conn_req->ssid, "__manual__") == 0) {
        conn_req->ssid[0] = '\0';
        get_form_value(body, "ssid_manual", conn_req->ssid, sizeof(conn_req->ssid));
    }
    free(body);

    if (conn_req->ssid[0] == '\0') {
        free(conn_req);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }

    if (conn_req->password[0] == '\0') {
        uint8_t count = storage_wifi_ap_count();
        for (uint8_t i = 0; i < count; i++) {
            char ssid[WIFI_SSID_MAX_LEN] = {0};
            char password[WIFI_PASS_MAX_LEN] = {0};
            if (storage_wifi_load_ap(i, ssid, sizeof(ssid), password, sizeof(password)) != ESP_OK) {
                continue;
            }
            if (strcmp(ssid, conn_req->ssid) == 0) {
                strncpy(conn_req->password, password, sizeof(conn_req->password) - 1);
                conn_req->password[sizeof(conn_req->password) - 1] = '\0';
                break;
            }
        }
    }

    s_connecting_busy = true;
    if (xTaskCreate(wifi_connect_task, "wifi_portal_conn", STACK_WIFI, conn_req,
                    TASK_PRIO_WIFI, NULL) != pdPASS) {
        s_connecting_busy = false;
        free(conn_req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "task create failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req,
        "<html><body><h3>已送出，裝置正在嘗試連線。</h3>"
        "<p>成功後 Core2 會自動切回 STA 模式。</p></body></html>");
}

static esp_err_t start_portal_http_server(void)
{
    if (s_httpd) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 12;
    config.stack_size = 8192;  /* esp_wifi_scan_start blocking + malloc 需要更大 stack */

    esp_err_t ret = httpd_start(&s_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Portal HTTP server 啟動失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t index_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = portal_index_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t wifi_uri = {
        .uri      = "/wifi",
        .method   = HTTP_POST,
        .handler  = portal_wifi_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t scan_uri = {
        .uri      = "/scan",
        .method   = HTTP_GET,
        .handler  = portal_scan_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_get_uri = {
        .uri      = "/ai",
        .method   = HTTP_GET,
        .handler  = portal_ai_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_post_uri = {
        .uri      = "/ai",
        .method   = HTTP_POST,
        .handler  = portal_ai_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_uri = {
        .uri      = "/stocks",
        .method   = HTTP_GET,
        .handler  = portal_stocks_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_add_uri = {
        .uri      = "/stocks/add",
        .method   = HTTP_POST,
        .handler  = portal_stocks_add_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_remove_uri = {
        .uri      = "/stocks/remove",
        .method   = HTTP_POST,
        .handler  = portal_stocks_remove_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t saved_aps_get_uri = {
        .uri      = "/saved_aps",
        .method   = HTTP_GET,
        .handler  = portal_saved_aps_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t saved_aps_remove_uri = {
        .uri      = "/saved_aps/remove",
        .method   = HTTP_POST,
        .handler  = portal_saved_aps_remove_post_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(s_httpd, &index_uri);
    httpd_register_uri_handler(s_httpd, &wifi_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &ai_get_uri);
    httpd_register_uri_handler(s_httpd, &ai_post_uri);
    httpd_register_uri_handler(s_httpd, &stocks_uri);
    httpd_register_uri_handler(s_httpd, &stocks_add_uri);
    httpd_register_uri_handler(s_httpd, &stocks_remove_uri);
    httpd_register_uri_handler(s_httpd, &saved_aps_get_uri);
    httpd_register_uri_handler(s_httpd, &saved_aps_remove_uri);

    return ESP_OK;
}

static void stop_portal_http_server(void)
{
    if (!s_httpd) return;
    httpd_stop(s_httpd);
    s_httpd = NULL;
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* STA 啟動不代表正在連線；只有呼叫 connect 時才進入 CONNECTING */
        notify_state(WIFI_STATE_DISCONNECTED);
        ESP_LOGI(TAG, "[%u ms] STA_START", (unsigned)esp_log_timestamp());

    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc = (const wifi_event_sta_disconnected_t *)event_data;
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);
        ESP_LOGW(TAG, "[%u ms] STA_DISCONNECTED reason=%d mode=%d retry=%d/%d",
                 (unsigned)esp_log_timestamp(),
                 disc ? (int)disc->reason : -1, (int)mode,
                 s_retry_count, MAX_RETRY);

        s_ip_str[0] = '\0';
        s_connected_ssid[0] = '\0';
        if (s_retry_count < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi 斷線，重試 %d/%d", s_retry_count, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            notify_state(WIFI_STATE_FAILED);
            ESP_LOGE(TAG, "WiFi 連線失敗");
        }

    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_ap_record_t ap_info = {0};

        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR,
                 IP2STR(&event->ip_info.ip));
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            strncpy(s_connected_ssid, (const char *)ap_info.ssid, sizeof(s_connected_ssid) - 1);
            s_connected_ssid[sizeof(s_connected_ssid) - 1] = '\0';
        } else {
            s_connected_ssid[0] = '\0';
        }
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        notify_state(WIFI_STATE_CONNECTED);
        ESP_LOGI(TAG, "[%u ms] STA_GOT_IP", (unsigned)esp_log_timestamp());
        ESP_LOGI(TAG, "WiFi 已連線 SSID=%s IP=%s",
                 (s_connected_ssid[0] != '\0') ? s_connected_ssid : "unknown",
                 s_ip_str);
    }
}

esp_err_t device_server_init(void)
{
    if (s_initialized) return ESP_OK;

    s_wifi_event_group = xEventGroupCreate();

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init 失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop 建立失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_initialized = true;
    ESP_LOGI(TAG, "Device Server 初始化完成");
    return ESP_OK;
}

esp_err_t device_server_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password,
            sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    s_retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    esp_wifi_disconnect();
    esp_wifi_connect();
    notify_state(WIFI_STATE_CONNECTING);

    /* 等待連線結果（30秒超時）*/
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(30000));
    if (bits & WIFI_CONNECTED_BIT) {
        /* 儲存成功的設定 */
        storage_wifi_add_ap(ssid, password);
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        notify_state(WIFI_STATE_FAILED);
    } else {
        /* WaitBits 逾時且未收到 fail bit，避免 UI 卡在「連線中」 */
        notify_state(WIFI_STATE_DISCONNECTED);
        ESP_LOGW(TAG, "WiFi 連線逾時");
    }
    return ESP_FAIL;
}

esp_err_t device_server_connect_any_saved(void)
{
    uint8_t count = storage_wifi_ap_count();
    if (count == 0) {
        ESP_LOGI(TAG, "無已儲存的 WiFi 設定");
        notify_state(WIFI_STATE_DISCONNECTED);
        return ESP_ERR_NOT_FOUND;
    }

    for (uint8_t i = 0; i < count; i++) {
        char ssid[WIFI_SSID_MAX_LEN] = {0};
        char password[WIFI_PASS_MAX_LEN] = {0};
        if (storage_wifi_load_ap(i, ssid, sizeof(ssid), password, sizeof(password)) != ESP_OK) {
            continue;
        }

        ESP_LOGI(TAG, "嘗試已儲存 AP %d/%d: %s", i + 1, count, ssid);
        if (device_server_connect(ssid, password) == ESP_OK) {
            return ESP_OK;
        }
    }

    notify_state(WIFI_STATE_FAILED);
    return ESP_FAIL;
}

esp_err_t device_server_connect_saved(void)
{
    return device_server_connect_any_saved();
}

esp_err_t device_server_disconnect(void)
{
    s_retry_count = MAX_RETRY;  /* 停止自動重試 */
    esp_wifi_disconnect();
    notify_state(WIFI_STATE_DISCONNECTED);
    return ESP_OK;
}

wifi_state_t device_server_get_state(void)
{
    return s_state;
}

bool device_server_is_connected(void)
{
    return (s_state == WIFI_STATE_CONNECTED);
}

const char *device_server_get_ip(void)
{
    return s_ip_str;
}

const char *device_server_get_connected_ssid(void)
{
    return s_connected_ssid;
}

void device_server_set_callback(wifi_state_cb_t cb)
{
    s_callback = cb;
}

esp_err_t device_server_start_provisioning_portal(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (s_portal_active) return ESP_OK;
    ESP_LOGI(TAG, "[%u ms] portal_start begin", (unsigned)esp_log_timestamp());

    if (!s_ap_netif) {
        ESP_LOGI(TAG, "[%u ms] portal_start create_default_wifi_ap",
                 (unsigned)esp_log_timestamp());
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "建立 AP netif 失敗");
            return ESP_FAIL;
        }
    }

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, s_portal_ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char *)ap_cfg.ap.password, s_portal_ap_password, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.ssid_len = strlen(s_portal_ap_ssid);
    ap_cfg.ap.channel = WIFI_PORTAL_AP_CHANNEL;
    ap_cfg.ap.max_connection = WIFI_PORTAL_MAX_STA;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_LOGI(TAG, "[%u ms] portal_start esp_wifi_set_mode(APSTA)",
             (unsigned)esp_log_timestamp());
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切換 APSTA 模式失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "[%u ms] portal_start esp_wifi_set_config(AP)",
             (unsigned)esp_log_timestamp());
    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "設定 AP 參數失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "[%u ms] portal_start httpd_start",
             (unsigned)esp_log_timestamp());
    ret = start_portal_http_server();
    if (ret != ESP_OK) return ret;

    s_portal_active = true;

    ESP_LOGI(TAG, "Portal 已啟動 AP=%s URL=%s", s_portal_ap_ssid, s_portal_url);
    return ESP_OK;
}

esp_err_t device_server_stop_provisioning_portal(void)
{
    if (!s_portal_active) return ESP_OK;
    ESP_LOGI(TAG, "[%u ms] portal_stop begin", (unsigned)esp_log_timestamp());

    ESP_LOGI(TAG, "[%u ms] portal_stop esp_wifi_set_mode(STA)",
             (unsigned)esp_log_timestamp());
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切換 STA 模式失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "[%u ms] portal_stop httpd_stop",
             (unsigned)esp_log_timestamp());
    stop_portal_http_server();

    s_portal_active = false;
    ESP_LOGI(TAG, "Portal 已停止");
    return ESP_OK;
}

bool device_server_is_provisioning_portal_active(void)
{
    return s_portal_active;
}

const char *device_server_get_provisioning_ap_ssid(void)
{
    return s_portal_ap_ssid;
}

const char *device_server_get_provisioning_ap_password(void)
{
    return s_portal_ap_password;
}

const char *device_server_get_provisioning_url(void)
{
    return s_portal_url;
}

const char *device_server_get_provisioning_ap_ip(void)
{
    static char ap_ip_str[20] = "0.0.0.0";
    esp_netif_ip_info_t ip_info = {0};

    if (!s_ap_netif) {
        return ap_ip_str;
    }

    if (esp_netif_get_ip_info(s_ap_netif, &ip_info) != ESP_OK) {
        return ap_ip_str;
    }

    snprintf(ap_ip_str, sizeof(ap_ip_str), IPSTR, IP2STR(&ip_info.ip));
    return ap_ip_str;
}

esp_err_t device_server_scan(wifi_ap_info_t *results, uint16_t *count,
                             uint16_t max_count)
{
    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) return ret;

    uint16_t ap_count = max_count;
    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!ap_records) return ESP_ERR_NO_MEM;

    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret == ESP_OK) {
        *count = (ap_count < max_count) ? ap_count : max_count;
        for (uint16_t i = 0; i < *count; i++) {
            strncpy(results[i].ssid, (char *)ap_records[i].ssid, 32);
            results[i].rssi = ap_records[i].rssi;
        }
    }
    free(ap_records);
    return ret;
}
