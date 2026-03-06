#include "portal_backend.h"
#include "stock_admin_service.h"
#include "at_time_admin_service.h"
#include "wifi_manager.h"

#include "storage.h"
#include "app_config.h"
#include "ai_provider.h"
#include "scheduler_service.h"
#include "telegram_bot.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PORTAL_BODY_MAX_LEN 4096
#define PORTAL_SCAN_MAX_APS 20
#define TELEGRAM_TOKEN_MAX_LEN 160
#define TELEGRAM_CHAT_ID_MAX_LEN 40
#define TELEGRAM_HTTP_BUF_INIT_SIZE 512
#define AI_TEST_RESP_MAX_LEN 1536
#define PORTAL_SCAN_CACHE_TTL_MS 15000
#define TELEGRAM_HTTP_DRAIN_TIMEOUT_MS 1200
#define SCHED_HTTP_DRAIN_TIMEOUT_MS 1200

static bool parse_ai_provider(const char *name, int *out_provider)
{
    if (!name || !out_provider) {
        return false;
    }
    if (strcmp(name, "gemini") == 0) {
        *out_provider = AI_PROVIDER_GEMINI;
        return true;
    }
    if (strcmp(name, "claude") == 0) {
        *out_provider = AI_PROVIDER_CLAUDE;
        return true;
    }
    if (strcmp(name, "openai") == 0) {
        *out_provider = AI_PROVIDER_OPENAI;
        return true;
    }
    return false;
}

static const char *ai_provider_name_from_id(int provider)
{
    switch (provider) {
    case AI_PROVIDER_GEMINI:
        return "gemini";
    case AI_PROVIDER_CLAUDE:
        return "claude";
    case AI_PROVIDER_OPENAI:
        return "openai";
    default:
        return "gemini";
    }
}

typedef struct {
    char ssid[33];
    char password[65];
} wifi_connect_req_t;

static const char *TAG = "portal_backend";
static esp_netif_t *s_ap_netif = NULL;
static httpd_handle_t s_httpd = NULL;
static bool s_portal_active = false;
static bool s_connecting_busy = false;
static wifi_ap_info_t s_scan_cache[PORTAL_SCAN_MAX_APS];
static uint16_t s_scan_cache_count = 0;
static int64_t s_scan_cache_ts_us = 0;

static char s_portal_ap_ssid[33] = WIFI_PORTAL_AP_SSID;
static char s_portal_ap_password[65] = WIFI_PORTAL_AP_PASSWORD;
static const char *s_portal_url = WIFI_PORTAL_URL;

static const char *CACHE_HTML_NO_STORE = "no-store, max-age=0";
static const char *CACHE_ASSET_PUBLIC = "public, max-age=86400";

extern const uint8_t portal_wifi_html_start[] asm("_binary_wifi_html_start");
extern const uint8_t portal_wifi_html_end[] asm("_binary_wifi_html_end");
extern const uint8_t portal_ai_html_start[] asm("_binary_ai_html_start");
extern const uint8_t portal_ai_html_end[] asm("_binary_ai_html_end");
extern const uint8_t portal_telegram_html_start[] asm("_binary_telegram_html_start");
extern const uint8_t portal_telegram_html_end[] asm("_binary_telegram_html_end");
extern const uint8_t portal_stocks_html_start[] asm("_binary_stocks_html_start");
extern const uint8_t portal_stocks_html_end[] asm("_binary_stocks_html_end");
extern const uint8_t portal_at_time_html_start[] asm("_binary_at_time_html_start");
extern const uint8_t portal_at_time_html_end[] asm("_binary_at_time_html_end");
extern const uint8_t portal_interval_html_start[] asm("_binary_interval_html_start");
extern const uint8_t portal_interval_html_end[] asm("_binary_interval_html_end");
extern const uint8_t portal_css_start[] asm("_binary_portal_css_start");
extern const uint8_t portal_css_end[] asm("_binary_portal_css_end");
extern const uint8_t portal_bootstrap_js_start[] asm("_binary_portal_bootstrap_js_start");
extern const uint8_t portal_bootstrap_js_end[] asm("_binary_portal_bootstrap_js_end");
extern const uint8_t tab_wifi_js_start[] asm("_binary_tab_wifi_js_start");
extern const uint8_t tab_wifi_js_end[] asm("_binary_tab_wifi_js_end");
extern const uint8_t tab_ai_js_start[] asm("_binary_tab_ai_js_start");
extern const uint8_t tab_ai_js_end[] asm("_binary_tab_ai_js_end");
extern const uint8_t tab_telegram_js_start[] asm("_binary_tab_telegram_js_start");
extern const uint8_t tab_telegram_js_end[] asm("_binary_tab_telegram_js_end");
extern const uint8_t tab_stocks_js_start[] asm("_binary_tab_stocks_js_start");
extern const uint8_t tab_stocks_js_end[] asm("_binary_tab_stocks_js_end");
extern const uint8_t tab_at_time_js_start[] asm("_binary_tab_at_time_js_start");
extern const uint8_t tab_at_time_js_end[] asm("_binary_tab_at_time_js_end");
extern const uint8_t tab_interval_js_start[] asm("_binary_tab_interval_js_start");
extern const uint8_t tab_interval_js_end[] asm("_binary_tab_interval_js_end");

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

            char tmp[PORTAL_BODY_MAX_LEN + 1] = {0};
            if (raw_len > PORTAL_BODY_MAX_LEN) {
                raw_len = PORTAL_BODY_MAX_LEN;
            }
            memcpy(tmp, p, raw_len);
            tmp[raw_len] = '\0';

            url_decode(out, out_len, tmp);
            return true;
        }

        p = strchr(p, '&');
        if (p) {
            p++;
        }
    }

    return false;
}

static bool form_flag_enabled(const char *body, const char *key)
{
    char value[16] = {0};
    if (!get_form_value(body, key, value, sizeof(value))) {
        return false;
    }

    return strcmp(value, "1") == 0 ||
           strcasecmp(value, "true") == 0 ||
           strcasecmp(value, "on") == 0;
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
                          : status == 502 ? "502 Bad Gateway"
                          : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json ? json : "{}");
}

static void mask_secret_tail4(const char *src, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (!src || src[0] == '\0') {
        return;
    }

    size_t len = strlen(src);
    const char *tail = src + (len > 4 ? (len - 4) : 0);
    snprintf(out, out_size, "****%s", tail);
}

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

typedef struct {
    char *buf;
    size_t capacity;
    size_t data_len;
    bool overflow;
} telegram_http_ctx_t;

static esp_err_t telegram_http_event_handler(esp_http_client_event_t *evt)
{
    telegram_http_ctx_t *ctx = (telegram_http_ctx_t *)evt->user_data;
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

static esp_err_t telegram_send_test_message(const char *token,
                                            const char *chat_id,
                                            const char *text)
{
    if (!token || !chat_id || !text || token[0] == '\0' || chat_id[0] == '\0' || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char encoded_chat[96] = {0};
    char encoded_text[384] = {0};
    char post_data[512] = {0};
    char url[320] = {0};
    int status_code = 0;
    char *resp_body = NULL;

    url_encode_component(chat_id, encoded_chat, sizeof(encoded_chat));
    url_encode_component(text, encoded_text, sizeof(encoded_text));
    snprintf(post_data, sizeof(post_data), "chat_id=%s&text=%s", encoded_chat, encoded_text);
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", token);

    telegram_http_ctx_t ctx = {0};
    ctx.buf = malloc(TELEGRAM_HTTP_BUF_INIT_SIZE);
    if (!ctx.buf) {
        return ESP_ERR_NO_MEM;
    }
    ctx.capacity = TELEGRAM_HTTP_BUF_INIT_SIZE;

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = telegram_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(ctx.buf);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, (int)strlen(post_data));

    esp_err_t ret = esp_http_client_perform(client);
    status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool success = false;
    if (ret == ESP_OK && !ctx.overflow && status_code == 200) {
        ctx.buf[ctx.data_len] = '\0';
        resp_body = ctx.buf;
        success = (strstr(resp_body, "\"ok\":true") != NULL);
    }
    if (resp_body == NULL) {
        free(ctx.buf);
    }
    free(resp_body);

    if (!success) {
        ESP_LOGW(TAG, "telegram test failed ret=%s status=%d overflow=%d",
                 esp_err_to_name(ret), status_code, ctx.overflow ? 1 : 0);
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }
    return ESP_OK;
}

static esp_err_t telegram_get_updates(const char *token, char **out_body, int *out_status)
{
    if (!token || token[0] == '\0' || !out_body) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_body = NULL;
    if (out_status) {
        *out_status = 0;
    }

    char url[384] = {0};
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates?limit=20&timeout=1", token);

    telegram_http_ctx_t ctx = {0};
    ctx.buf = malloc(TELEGRAM_HTTP_BUF_INIT_SIZE);
    if (!ctx.buf) {
        return ESP_ERR_NO_MEM;
    }
    ctx.capacity = TELEGRAM_HTTP_BUF_INIT_SIZE;

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = telegram_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_err_t ret = ESP_FAIL;
    int status_code = 0;
    const int max_attempts = 2;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) {
            free(ctx.buf);
            return ESP_FAIL;
        }
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        ctx.data_len = 0;
        ctx.overflow = false;
        ret = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (ret == ESP_OK && !ctx.overflow) {
            break;
        }
        if (attempt < max_attempts && ret == ESP_ERR_HTTP_CONNECT) {
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }
        break;
    }
    if (out_status) {
        *out_status = status_code;
    }

    if (ret != ESP_OK || ctx.overflow) {
        free(ctx.buf);
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }
    ctx.buf[ctx.data_len] = '\0';
    *out_body = ctx.buf;
    return ESP_OK;
}

static esp_err_t telegram_get_me(const char *token, char *bot_name, size_t bot_name_len,
                                 char *bot_username, size_t bot_username_len, int *out_status)
{
    if (!token || token[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!bot_name || bot_name_len == 0 || !bot_username || bot_username_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    bot_name[0] = '\0';
    bot_username[0] = '\0';
    if (out_status) {
        *out_status = 0;
    }

    char url[320] = {0};
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getMe", token);

    telegram_http_ctx_t ctx = {0};
    ctx.buf = malloc(TELEGRAM_HTTP_BUF_INIT_SIZE);
    if (!ctx.buf) {
        return ESP_ERR_NO_MEM;
    }
    ctx.capacity = TELEGRAM_HTTP_BUF_INIT_SIZE;

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = telegram_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_err_t ret = ESP_FAIL;
    int status_code = 0;
    const int max_attempts = 2;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) {
            free(ctx.buf);
            return ESP_FAIL;
        }
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        ctx.data_len = 0;
        ctx.overflow = false;
        ret = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (ret == ESP_OK && !ctx.overflow) {
            break;
        }
        if (attempt < max_attempts && ret == ESP_ERR_HTTP_CONNECT) {
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }
        break;
    }
    if (out_status) {
        *out_status = status_code;
    }

    if (ret != ESP_OK || ctx.overflow) {
        free(ctx.buf);
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }

    ctx.buf[ctx.data_len] = '\0';
    cJSON *root = cJSON_Parse(ctx.buf);
    free(ctx.buf);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *first_name = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "first_name") : NULL;
    cJSON *username = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "username") : NULL;
    bool api_ok = cJSON_IsTrue(ok);

    if (!api_ok) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    bool parse_ok = false;
    if (cJSON_IsString(first_name) && first_name->valuestring && first_name->valuestring[0] != '\0') {
        strlcpy(bot_name, first_name->valuestring, bot_name_len);
        parse_ok = true;
    } else if (cJSON_IsString(username) && username->valuestring && username->valuestring[0] != '\0') {
        strlcpy(bot_name, username->valuestring, bot_name_len);
        parse_ok = true;
    }

    if (cJSON_IsString(username) && username->valuestring) {
        strlcpy(bot_username, username->valuestring, bot_username_len);
    }

    cJSON_Delete(root);

    if (!parse_ok) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void wifi_connect_task(void *arg)
{
    wifi_connect_req_t *req = (wifi_connect_req_t *)arg;
    if (!req) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Portal 提交配網，嘗試連線 SSID=%s", req->ssid);
    esp_err_t ret = wifi_manager_connect(req->ssid, req->password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Portal 配網成功");
        if (s_portal_active) {
            esp_err_t stop_ret = portal_backend_stop();
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

static esp_err_t portal_scan_get_handler(httpd_req_t *req)
{
    int64_t start_us = esp_timer_get_time();
    int64_t now_us = start_us;
    bool use_cache = false;
    wifi_ap_info_t fresh_infos[PORTAL_SCAN_MAX_APS] = {0};
    const wifi_ap_info_t *ap_infos = fresh_infos;
    uint16_t count = PORTAL_SCAN_MAX_APS;

    if (s_scan_cache_count > 0 &&
        (now_us - s_scan_cache_ts_us) <= ((int64_t)PORTAL_SCAN_CACHE_TTL_MS * 1000LL)) {
        use_cache = true;
        ap_infos = s_scan_cache;
        count = s_scan_cache_count;
    } else {
        esp_err_t scan_ret = wifi_manager_scan(fresh_infos, &count, PORTAL_SCAN_MAX_APS);
        if (scan_ret != ESP_OK) {
            ESP_LOGW(TAG, "GET /scan failed err=%s elapsed=%lldms",
                     esp_err_to_name(scan_ret),
                     (long long)((esp_timer_get_time() - start_us) / 1000LL));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
            return scan_ret;
        }
        if (count > 0) {
            memcpy(s_scan_cache, fresh_infos, count * sizeof(wifi_ap_info_t));
        }
        s_scan_cache_count = count;
        s_scan_cache_ts_us = now_us;
    }

    char *buf = malloc(2048);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    char esc[68];
    size_t pos = 0;
    buf[pos++] = '[';
    for (uint16_t i = 0; i < count && pos < 2040; i++) {
        json_escape(esc, sizeof(esc), ap_infos[i].ssid);
        int n = snprintf(buf + pos, 2048 - pos,
                         "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                         i > 0 ? "," : "", esc, (int)ap_infos[i].rssi);
        if (n <= 0 || (size_t)n >= 2048 - pos) {
            break;
        }
        pos += (size_t)n;
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, buf, (ssize_t)pos);
    ESP_LOGI(TAG, "GET /scan mode=%s count=%u elapsed=%lldms",
             use_cache ? "cache" : "fresh",
             (unsigned)count,
             (long long)((esp_timer_get_time() - start_us) / 1000LL));
    free(buf);
    return ret;
}

static esp_err_t portal_ai_get_handler(httpd_req_t *req)
{
    char gemini_key[128] = {0};
    char claude_key[128] = {0};
    char openai_key[128] = {0};
    char global_prompt[513] = {0};
    uint8_t provider = (uint8_t)AI_PROVIDER_GEMINI;
    char gemini_masked[24] = {0};
    char claude_masked[24] = {0};
    char openai_masked[24] = {0};
    char e_gemini_masked[64] = {0};
    char e_claude_masked[64] = {0};
    char e_openai_masked[64] = {0};
    char e_global_prompt[1100] = {0};

    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key, sizeof(gemini_key));
    if (gemini_key[0] == '\0') {
        storage_ai_load_key(gemini_key, sizeof(gemini_key));
    }
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key, sizeof(claude_key));
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key, sizeof(openai_key));
    storage_ai_load_global_prompt(global_prompt, sizeof(global_prompt));
    storage_ai_load_provider(&provider);
    if (provider > (uint8_t)AI_PROVIDER_OPENAI) {
        provider = (uint8_t)AI_PROVIDER_GEMINI;
    }

    mask_secret_tail4(gemini_key, gemini_masked, sizeof(gemini_masked));
    mask_secret_tail4(claude_key, claude_masked, sizeof(claude_masked));
    mask_secret_tail4(openai_key, openai_masked, sizeof(openai_masked));
    json_escape(e_gemini_masked, sizeof(e_gemini_masked), gemini_masked);
    json_escape(e_claude_masked, sizeof(e_claude_masked), claude_masked);
    json_escape(e_openai_masked, sizeof(e_openai_masked), openai_masked);
    json_escape(e_global_prompt, sizeof(e_global_prompt), global_prompt);

    char *json = malloc(1800);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    int n = snprintf(json, 1800,
                     "{\"gemini_configured\":%s,\"claude_configured\":%s,\"openai_configured\":%s,"
                     "\"gemini_key_masked\":\"%s\",\"claude_key_masked\":\"%s\",\"openai_key_masked\":\"%s\","
                     "\"provider\":\"%s\",\"global_prompt\":\"%s\"}",
                     gemini_key[0] != '\0' ? "true" : "false",
                     claude_key[0] != '\0' ? "true" : "false",
                     openai_key[0] != '\0' ? "true" : "false",
                     e_gemini_masked, e_claude_masked, e_openai_masked,
                     ai_provider_name_from_id((int)provider),
                     e_global_prompt);

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
    char global_prompt[1025] = {0};
    char provider_name[16] = {0};
    int selected_provider = -1;

    bool has_provider = get_form_value(body, "provider", provider_name, sizeof(provider_name));
    bool has_gemini = get_form_value(body, "gemini_key", gemini_key, sizeof(gemini_key));
    bool has_claude = get_form_value(body, "claude_key", claude_key, sizeof(claude_key));
    bool has_openai = get_form_value(body, "openai_key", openai_key, sizeof(openai_key));
    bool has_global_prompt = get_form_value(body, "global_prompt", global_prompt, sizeof(global_prompt));
    bool clear_gemini = form_flag_enabled(body, "clear_gemini");
    bool clear_claude = form_flag_enabled(body, "clear_claude");
    bool clear_openai = form_flag_enabled(body, "clear_openai");

    if (!has_provider) {
        free(body);
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"missing_provider\"}");
    }
    if (!parse_ai_provider(provider_name, &selected_provider)) {
        free(body);
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"invalid_provider\"}");
    }
    if (has_global_prompt && strlen(global_prompt) > 300) {
        free(body);
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"prompt_too_long\"}");
    }
    free(body);

    if ((clear_gemini && has_gemini && gemini_key[0] != '\0') ||
        (clear_claude && has_claude && claude_key[0] != '\0') ||
        (clear_openai && has_openai && openai_key[0] != '\0')) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"conflict_clear_and_set\"}");
    }

    if (clear_gemini) {
        if (storage_ai_clear_provider_key((uint8_t)AI_PROVIDER_GEMINI) != ESP_OK ||
            storage_ai_clear_legacy_key() != ESP_OK) {
            return send_json_response(req, 500, "{\"ok\":false,\"error\":\"clear_failed_gemini\"}");
        }
    }
    if (clear_claude) {
        if (storage_ai_clear_provider_key((uint8_t)AI_PROVIDER_CLAUDE) != ESP_OK) {
            return send_json_response(req, 500, "{\"ok\":false,\"error\":\"clear_failed_claude\"}");
        }
    }
    if (clear_openai) {
        if (storage_ai_clear_provider_key((uint8_t)AI_PROVIDER_OPENAI) != ESP_OK) {
            return send_json_response(req, 500, "{\"ok\":false,\"error\":\"clear_failed_openai\"}");
        }
    }

    if (has_gemini && gemini_key[0] != '\0' &&
        storage_ai_save_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed_gemini\"}");
    }
    if (has_claude && claude_key[0] != '\0' &&
        storage_ai_save_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed_claude\"}");
    }
    if (has_openai && openai_key[0] != '\0' &&
        storage_ai_save_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed_openai\"}");
    }
    if (storage_ai_save_provider((uint8_t)selected_provider) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed_provider\"}");
    }
    if (has_global_prompt && storage_ai_save_global_prompt(global_prompt) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed_prompt\"}");
    }

    char json[224] = {0};
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"provider\":\"%s\",\"global_prompt_updated\":%s,"
             "\"cleared\":{\"gemini\":%s,\"claude\":%s,\"openai\":%s}}",
             ai_provider_name_from_id(selected_provider),
             has_global_prompt ? "true" : "false",
             clear_gemini ? "true" : "false",
             clear_claude ? "true" : "false",
             clear_openai ? "true" : "false");
    return send_json_response(req, 200, json);
}

static esp_err_t portal_ai_test_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    char provider[16] = {0};
    char provided_api_key[128] = {0};
    bool has_provided_api_key = false;
    int selected_provider = -1; /* -1 = test all providers */

    if (req && req->content_len > 0) {
        if (read_request_body_alloc(req, &body) != ESP_OK) {
            return send_json_response(req, 400, "{\"ok\":false,\"error\":\"invalid_request\"}");
        }

        if (get_form_value(body, "provider", provider, sizeof(provider))) {
            if (!parse_ai_provider(provider, &selected_provider)) {
                free(body);
                return send_json_response(req, 400, "{\"ok\":false,\"error\":\"invalid_provider\"}");
            }
        }
        has_provided_api_key = get_form_value(body, "api_key", provided_api_key, sizeof(provided_api_key));
        free(body);
    }

    char gemini_key[128] = {0};
    char claude_key[128] = {0};
    char openai_key[128] = {0};
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int gemini_status = 0;
    int claude_status = 0;
    int openai_status = 0;
    bool gemini_ok = false;
    bool claude_ok = false;
    bool openai_ok = false;
    const char *gemini_error = "";
    const char *claude_error = "";
    const char *openai_error = "";

    if (selected_provider < 0 || selected_provider == AI_PROVIDER_GEMINI) {
        if (selected_provider == AI_PROVIDER_GEMINI && has_provided_api_key && provided_api_key[0] != '\0') {
            strlcpy(gemini_key, provided_api_key, sizeof(gemini_key));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key, sizeof(gemini_key));
            if (gemini_key[0] == '\0') {
                storage_ai_load_key(gemini_key, sizeof(gemini_key));
            }
        }
    }
    if (selected_provider < 0 || selected_provider == AI_PROVIDER_CLAUDE) {
        if (selected_provider == AI_PROVIDER_CLAUDE && has_provided_api_key && provided_api_key[0] != '\0') {
            strlcpy(claude_key, provided_api_key, sizeof(claude_key));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key, sizeof(claude_key));
        }
    }
    if (selected_provider < 0 || selected_provider == AI_PROVIDER_OPENAI) {
        if (selected_provider == AI_PROVIDER_OPENAI && has_provided_api_key && provided_api_key[0] != '\0') {
            strlcpy(openai_key, provided_api_key, sizeof(openai_key));
        } else {
            storage_ai_load_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key, sizeof(openai_key));
        }
    }

    if ((selected_provider == AI_PROVIDER_GEMINI && gemini_key[0] == '\0') ||
        (selected_provider == AI_PROVIDER_CLAUDE && claude_key[0] == '\0') ||
        (selected_provider == AI_PROVIDER_OPENAI && openai_key[0] == '\0') ||
        (selected_provider < 0 && gemini_key[0] == '\0' && claude_key[0] == '\0' && openai_key[0] == '\0')) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"missing_config\"}");
    }

    if (selected_provider < 0 || selected_provider == AI_PROVIDER_GEMINI) {
        if (gemini_key[0] != '\0') {
            esp_err_t ret = ai_provider_test_key((ai_provider_type_t)AI_PROVIDER_GEMINI,
                                                 gemini_key,
                                                 &gemini_status);
            gemini_ok = (ret == ESP_OK && gemini_status == 200);
            gemini_error = (ret != ESP_OK) ? "network_error" : (gemini_ok ? "" : "http_error");
            if (gemini_ok) {
                passed++;
            } else {
                failed++;
            }
        } else {
            skipped++;
        }
    }

    if (selected_provider < 0 || selected_provider == AI_PROVIDER_CLAUDE) {
        if (claude_key[0] != '\0') {
            esp_err_t ret = ai_provider_test_key((ai_provider_type_t)AI_PROVIDER_CLAUDE,
                                                 claude_key,
                                                 &claude_status);
            claude_ok = (ret == ESP_OK && claude_status == 200);
            claude_error = (ret != ESP_OK) ? "network_error" : (claude_ok ? "" : "http_error");
            if (claude_ok) {
                passed++;
            } else {
                failed++;
            }
        } else {
            skipped++;
        }
    }

    if (selected_provider < 0 || selected_provider == AI_PROVIDER_OPENAI) {
        if (openai_key[0] != '\0') {
            esp_err_t ret = ai_provider_test_key((ai_provider_type_t)AI_PROVIDER_OPENAI,
                                                 openai_key,
                                                 &openai_status);
            openai_ok = (ret == ESP_OK && openai_status == 200);
            openai_error = (ret != ESP_OK) ? "network_error" : (openai_ok ? "" : "http_error");
            if (openai_ok) {
                passed++;
            } else {
                failed++;
            }
        } else {
            skipped++;
        }
    }

    ESP_LOGI(TAG, "AI test result: passed=%d failed=%d skipped=%d", passed, failed, skipped);

    char json[AI_TEST_RESP_MAX_LEN] = {0};
    if (selected_provider == AI_PROVIDER_GEMINI) {
        snprintf(json, sizeof(json),
                 "{\"ok\":true,\"summary\":{\"passed\":%d,\"failed\":%d,\"skipped\":%d},"
                 "\"results\":["
                 "{\"provider\":\"gemini\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"}"
                 "]}",
                 passed, failed, skipped,
                 gemini_key[0] != '\0' ? "true" : "false",
                 gemini_ok ? "true" : "false",
                 gemini_status,
                 gemini_error);
    } else if (selected_provider == AI_PROVIDER_CLAUDE) {
        snprintf(json, sizeof(json),
                 "{\"ok\":true,\"summary\":{\"passed\":%d,\"failed\":%d,\"skipped\":%d},"
                 "\"results\":["
                 "{\"provider\":\"claude\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"}"
                 "]}",
                 passed, failed, skipped,
                 claude_key[0] != '\0' ? "true" : "false",
                 claude_ok ? "true" : "false",
                 claude_status,
                 claude_error);
    } else if (selected_provider == AI_PROVIDER_OPENAI) {
        snprintf(json, sizeof(json),
                 "{\"ok\":true,\"summary\":{\"passed\":%d,\"failed\":%d,\"skipped\":%d},"
                 "\"results\":["
                 "{\"provider\":\"openai\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"}"
                 "]}",
                 passed, failed, skipped,
                 openai_key[0] != '\0' ? "true" : "false",
                 openai_ok ? "true" : "false",
                 openai_status,
                 openai_error);
    } else {
        snprintf(json, sizeof(json),
                 "{\"ok\":true,\"summary\":{\"passed\":%d,\"failed\":%d,\"skipped\":%d},"
                 "\"results\":["
                 "{\"provider\":\"gemini\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"},"
                 "{\"provider\":\"claude\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"},"
                 "{\"provider\":\"openai\",\"configured\":%s,\"ok\":%s,\"status\":%d,\"error\":\"%s\"}"
                 "]}",
                 passed, failed, skipped,
                 gemini_key[0] != '\0' ? "true" : "false",
                 gemini_ok ? "true" : "false",
                 gemini_status,
                 gemini_error,
                 claude_key[0] != '\0' ? "true" : "false",
                 claude_ok ? "true" : "false",
                 claude_status,
                 claude_error,
                 openai_key[0] != '\0' ? "true" : "false",
                 openai_ok ? "true" : "false",
                 openai_status,
                 openai_error);
    }

    return send_json_response(req, 200, json);
}

static esp_err_t portal_telegram_get_handler(httpd_req_t *req)
{
    bool enabled = false;
    char chat_id[TELEGRAM_CHAT_ID_MAX_LEN] = {0};
    char token[TELEGRAM_TOKEN_MAX_LEN] = {0};
    char token_masked[24] = {0};
    char e_chat_id[96] = {0};
    char e_token_masked[64] = {0};

    storage_tg_load_enabled(&enabled);
    storage_tg_load_chat_id(chat_id, sizeof(chat_id));
    storage_tg_load_bot_token(token, sizeof(token));
    mask_secret_tail4(token, token_masked, sizeof(token_masked));

    json_escape(e_chat_id, sizeof(e_chat_id), chat_id);
    json_escape(e_token_masked, sizeof(e_token_masked), token_masked);

    char json[256] = {0};
    snprintf(json, sizeof(json),
             "{\"enabled\":%s,\"chat_id\":\"%s\",\"token_masked\":\"%s\"}",
             enabled ? "true" : "false",
             e_chat_id,
             e_token_masked);
    return send_json_response(req, 200, json);
}

static esp_err_t portal_telegram_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    if (read_request_body_alloc(req, &body) != ESP_OK) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"invalid_request\"}");
    }

    char enabled_raw[8] = {0};
    char token[TELEGRAM_TOKEN_MAX_LEN] = {0};
    char chat_id[TELEGRAM_CHAT_ID_MAX_LEN] = {0};

    bool has_enabled = get_form_value(body, "enabled", enabled_raw, sizeof(enabled_raw));
    bool has_token = get_form_value(body, "bot_token", token, sizeof(token));
    bool has_chat_id = get_form_value(body, "chat_id", chat_id, sizeof(chat_id));
    free(body);

    bool enabled = has_enabled && (strcmp(enabled_raw, "1") == 0 || strcasecmp(enabled_raw, "true") == 0);
    if (storage_tg_save_enabled(enabled) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed\"}");
    }
    /* chat_id 欄位若有提交，允許存空字串（用於清除設定） */
    if (has_chat_id && storage_tg_save_chat_id(chat_id) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed\"}");
    }
    /* token 保留「空字串不覆寫」行為，避免誤清空 */
    if (has_token && token[0] != '\0' && storage_tg_save_bot_token(token) != ESP_OK) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"save_failed\"}");
    }

    return send_json_response(req, 200, "{\"ok\":true}");
}

static bool is_sta_connected(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return false;
    }
    /* Also verify STA has a valid IP (not just associated) */
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) {
        return false;
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(sta, &ip) != ESP_OK || ip.ip.addr == 0) {
        return false;
    }
    return true;
}

static bool begin_telegram_http_exclusive(bool *out_should_resume_tg,
                                          bool *out_should_resume_sched)
{
    if (out_should_resume_tg) {
        *out_should_resume_tg = false;
    }
    if (out_should_resume_sched) {
        *out_should_resume_sched = false;
    }

    esp_err_t sched_pause_ret = scheduler_service_pause_quote_polling();
    if (sched_pause_ret == ESP_OK) {
        if (out_should_resume_sched) {
            *out_should_resume_sched = true;
        }
    } else {
        ESP_LOGW(TAG, "scheduler quote pause failed: %s", esp_err_to_name(sched_pause_ret));
    }
    esp_err_t sched_idle_ret = scheduler_service_wait_quote_fetch_idle(SCHED_HTTP_DRAIN_TIMEOUT_MS);
    if (sched_idle_ret != ESP_OK) {
        ESP_LOGW(TAG, "scheduler quote fetch not idle: %s", esp_err_to_name(sched_idle_ret));
    }

    if (!telegram_bot_is_running()) {
        return true;
    }

    bool was_paused = telegram_bot_is_polling_paused();
    if (!was_paused) {
        esp_err_t pause_ret = telegram_bot_pause_polling();
        if (pause_ret != ESP_OK) {
            ESP_LOGW(TAG, "telegram polling pause failed: %s", esp_err_to_name(pause_ret));
        } else if (out_should_resume_tg) {
            *out_should_resume_tg = true;
        }
    }

    esp_err_t idle_ret = telegram_bot_wait_http_idle(TELEGRAM_HTTP_DRAIN_TIMEOUT_MS);
    if (idle_ret != ESP_OK) {
        ESP_LOGW(TAG, "telegram http not idle before portal request: %s", esp_err_to_name(idle_ret));
    }
    return true;
}

static void end_telegram_http_exclusive(bool should_resume_tg, bool should_resume_sched)
{
    if (should_resume_tg) {
        esp_err_t resume_ret = telegram_bot_resume_polling();
        if (resume_ret != ESP_OK) {
            ESP_LOGW(TAG, "telegram polling resume failed: %s", esp_err_to_name(resume_ret));
        }
    }
    if (should_resume_sched) {
        esp_err_t sched_resume_ret = scheduler_service_resume_quote_polling();
        if (sched_resume_ret != ESP_OK) {
            ESP_LOGW(TAG, "scheduler quote resume failed: %s", esp_err_to_name(sched_resume_ret));
        }
    }
}

static esp_err_t portal_telegram_test_post_handler(httpd_req_t *req)
{
    (void)req;
    char token[TELEGRAM_TOKEN_MAX_LEN] = {0};
    char chat_id[TELEGRAM_CHAT_ID_MAX_LEN] = {0};
    bool enabled = false;

    storage_tg_load_enabled(&enabled);
    storage_tg_load_bot_token(token, sizeof(token));
    storage_tg_load_chat_id(chat_id, sizeof(chat_id));

    if (!enabled || token[0] == '\0' || chat_id[0] == '\0') {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"missing_config\"}");
    }

    if (!is_sta_connected()) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"no_internet\"}");
    }

    bool should_resume_tg = false;
    bool should_resume_sched = false;
    begin_telegram_http_exclusive(&should_resume_tg, &should_resume_sched);
    esp_err_t ret = telegram_send_test_message(token, chat_id, "Core2 Telegram test: settings OK");
    end_telegram_http_exclusive(should_resume_tg, should_resume_sched);
    if (ret != ESP_OK) {
        return send_json_response(req, 502, "{\"ok\":false,\"error\":\"send_failed\"}");
    }

    return send_json_response(req, 200, "{\"ok\":true}");
}

static esp_err_t portal_telegram_check_post_handler(httpd_req_t *req)
{
    char token[TELEGRAM_TOKEN_MAX_LEN] = {0};
    if (req->content_len > 0) {
        char *body = NULL;
        if (read_request_body_alloc(req, &body) != ESP_OK) {
            return send_json_response(req, 400, "{\"ok\":false,\"error\":\"invalid_request\"}");
        }
        get_form_value(body, "bot_token", token, sizeof(token));
        free(body);
    }

    if (token[0] == '\0') {
        storage_tg_load_bot_token(token, sizeof(token));
    }

    if (token[0] == '\0') {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"missing_token\"}");
    }

    if (!is_sta_connected()) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"no_internet\"}");
    }

    char bot_name[64] = {0};
    char bot_username[64] = {0};
    int status_code = 0;
    bool should_resume_tg = false;
    bool should_resume_sched = false;
    begin_telegram_http_exclusive(&should_resume_tg, &should_resume_sched);
    esp_err_t ret = telegram_get_me(token, bot_name, sizeof(bot_name), bot_username, sizeof(bot_username), &status_code);
    end_telegram_http_exclusive(should_resume_tg, should_resume_sched);
    if (ret != ESP_OK) {
        if (status_code == 401 || status_code == 404) {
            return send_json_response(req, 502, "{\"ok\":false,\"error\":\"invalid_token\"}");
        }
        if (ret == ESP_ERR_HTTP_CONNECT || status_code == 0) {
            char err_buf[128] = {0};
            snprintf(err_buf, sizeof(err_buf),
                     "{\"ok\":false,\"error\":\"connect_failed\",\"detail\":\"http=%d esp=%s\"}",
                     status_code, esp_err_to_name(ret));
            return send_json_response(req, 502, err_buf);
        }
        ESP_LOGW(TAG, "telegram getMe failed: ret=%s status=%d", esp_err_to_name(ret), status_code);
        char err_buf[128] = {0};
        snprintf(err_buf, sizeof(err_buf),
                 "{\"ok\":false,\"error\":\"check_failed\",\"detail\":\"http=%d esp=%s\"}",
                 status_code, esp_err_to_name(ret));
        return send_json_response(req, 502, err_buf);
    }

    char e_name[96] = {0};
    char e_username[96] = {0};
    json_escape(e_name, sizeof(e_name), bot_name);
    json_escape(e_username, sizeof(e_username), bot_username);

    char json[320] = {0};
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"bot_name\":\"%s\",\"username\":\"%s\"}",
             e_name, e_username);
    return send_json_response(req, 200, json);
}

static cJSON *extract_chat_obj_from_update(cJSON *update_item)
{
    const char *keys[] = {"message", "edited_message", "channel_post", "edited_channel_post"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        cJSON *msg = cJSON_GetObjectItem(update_item, keys[i]);
        if (!cJSON_IsObject(msg)) {
            continue;
        }
        cJSON *chat = cJSON_GetObjectItem(msg, "chat");
        if (cJSON_IsObject(chat)) {
            return chat;
        }
    }
    return NULL;
}

static esp_err_t portal_telegram_chats_get_handler(httpd_req_t *req)
{
    char token[TELEGRAM_TOKEN_MAX_LEN] = {0};
    storage_tg_load_bot_token(token, sizeof(token));
    if (token[0] == '\0') {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"missing_token\"}");
    }

    if (!is_sta_connected()) {
        return send_json_response(req, 400, "{\"ok\":false,\"error\":\"no_internet\"}");
    }

    int status_code = 0;
    char *resp_body = NULL;
    bool should_resume_tg = false;
    bool should_resume_sched = false;
    begin_telegram_http_exclusive(&should_resume_tg, &should_resume_sched);
    esp_err_t ret = telegram_get_updates(token, &resp_body, &status_code);
    end_telegram_http_exclusive(should_resume_tg, should_resume_sched);
    if (ret != ESP_OK || !resp_body || status_code != 200) {
        ESP_LOGW(TAG, "telegram getUpdates failed: ret=%s status=%d body=%s",
                 esp_err_to_name(ret), status_code, resp_body ? resp_body : "(null)");
        free(resp_body);
        char err_buf[128];
        if (status_code == 401) {
            snprintf(err_buf, sizeof(err_buf),
                     "{\"ok\":false,\"error\":\"invalid_token\"}");
        } else if (ret == ESP_ERR_HTTP_CONNECT || status_code == 0) {
            snprintf(err_buf, sizeof(err_buf),
                     "{\"ok\":false,\"error\":\"no_internet\",\"detail\":\"http=%d esp=%s\"}",
                     status_code, esp_err_to_name(ret));
        } else {
            snprintf(err_buf, sizeof(err_buf),
                     "{\"ok\":false,\"error\":\"updates_failed\",\"detail\":\"http=%d esp=%s\"}",
                     status_code, esp_err_to_name(ret));
        }
        return send_json_response(req, 502, err_buf);
    }

    cJSON *root = cJSON_Parse(resp_body);
    free(resp_body);
    if (!root) {
        return send_json_response(req, 502, "{\"ok\":false,\"error\":\"bad_json\"}");
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return send_json_response(req, 502, "{\"ok\":false,\"error\":\"bad_result\"}");
    }

    cJSON *out_root = cJSON_CreateObject();
    cJSON *items = cJSON_AddArrayToObject(out_root, "items");
    cJSON_AddBoolToObject(out_root, "ok", true);

    const int max_items = 20;
    char seen_ids[max_items][24];
    int seen_count = 0;
    memset(seen_ids, 0, sizeof(seen_ids));

    int count = cJSON_GetArraySize(result);
    for (int i = 0; i < count && seen_count < max_items; i++) {
        cJSON *update_item = cJSON_GetArrayItem(result, i);
        if (!cJSON_IsObject(update_item)) {
            continue;
        }

        cJSON *chat = extract_chat_obj_from_update(update_item);
        if (!chat) {
            continue;
        }

        cJSON *chat_id_obj = cJSON_GetObjectItem(chat, "id");
        char chat_id[24] = {0};
        if (cJSON_IsNumber(chat_id_obj)) {
            snprintf(chat_id, sizeof(chat_id), "%.0f", chat_id_obj->valuedouble);
        } else if (cJSON_IsString(chat_id_obj) && chat_id_obj->valuestring) {
            strlcpy(chat_id, chat_id_obj->valuestring, sizeof(chat_id));
        } else {
            continue;
        }

        bool duplicated = false;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen_ids[s], chat_id) == 0) {
                duplicated = true;
                break;
            }
        }
        if (duplicated) {
            continue;
        }
        strlcpy(seen_ids[seen_count], chat_id, sizeof(seen_ids[seen_count]));
        seen_count++;

        char label[96] = {0};
        cJSON *title = cJSON_GetObjectItem(chat, "title");
        cJSON *username = cJSON_GetObjectItem(chat, "username");
        cJSON *first_name = cJSON_GetObjectItem(chat, "first_name");
        cJSON *last_name = cJSON_GetObjectItem(chat, "last_name");
        cJSON *type = cJSON_GetObjectItem(chat, "type");
        const char *type_str = (cJSON_IsString(type) && type->valuestring) ? type->valuestring : "unknown";

        if (cJSON_IsString(title) && title->valuestring) {
            snprintf(label, sizeof(label), "%s (%s)", title->valuestring, type_str);
        } else if (cJSON_IsString(first_name) && first_name->valuestring) {
            if (cJSON_IsString(last_name) && last_name->valuestring && last_name->valuestring[0] != '\0') {
                snprintf(label, sizeof(label), "%s %s (%s)", first_name->valuestring, last_name->valuestring, type_str);
            } else {
                snprintf(label, sizeof(label), "%s (%s)", first_name->valuestring, type_str);
            }
        } else if (cJSON_IsString(username) && username->valuestring) {
            snprintf(label, sizeof(label), "@%s (%s)", username->valuestring, type_str);
        } else {
            snprintf(label, sizeof(label), "%s (%s)", chat_id, type_str);
        }

        cJSON *one = cJSON_CreateObject();
        cJSON_AddStringToObject(one, "chat_id", chat_id);
        cJSON_AddStringToObject(one, "label", label);
        cJSON_AddStringToObject(one, "type", type_str);
        cJSON_AddItemToArray(items, one);
    }

    char *out_str = cJSON_PrintUnformatted(out_root);
    cJSON_Delete(out_root);
    cJSON_Delete(root);

    if (!out_str) {
        return send_json_response(req, 500, "{\"ok\":false,\"error\":\"encode_failed\"}");
    }

    esp_err_t send_ret = send_json_response(req, 200, out_str);
    free(out_str);
    return send_ret;
}

static esp_err_t send_embedded_asset(httpd_req_t *req,
                                     const uint8_t *start,
                                     const uint8_t *end,
                                     const char *content_type,
                                     const char *cache_control)
{
    if (!req || !start || !end || !content_type || !cache_control || end < start) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = (size_t)(end - start);
    if (len > 0 && start[len - 1] == '\0') {
        len--;
    }
    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Cache-Control", cache_control);
    return httpd_resp_send(req, (const char *)start, (ssize_t)len);
}

static esp_err_t send_portal_page(httpd_req_t *req, const uint8_t *start, const uint8_t *end)
{
    return send_embedded_asset(req, start, end, "text/html; charset=utf-8", CACHE_HTML_NO_STORE);
}

static esp_err_t portal_root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/wifi");
    httpd_resp_set_hdr(req, "Cache-Control", CACHE_HTML_NO_STORE);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t portal_wifi_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_wifi_html_start, portal_wifi_html_end);
}

static esp_err_t portal_ai_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_ai_html_start, portal_ai_html_end);
}

static esp_err_t portal_telegram_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_telegram_html_start, portal_telegram_html_end);
}

static esp_err_t portal_stocks_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_stocks_html_start, portal_stocks_html_end);
}

static esp_err_t portal_at_time_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_at_time_html_start, portal_at_time_html_end);
}

static esp_err_t portal_interval_page_get_handler(httpd_req_t *req)
{
    return send_portal_page(req, portal_interval_html_start, portal_interval_html_end);
}

static esp_err_t portal_css_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, portal_css_start, portal_css_end,
                               "text/css; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_bootstrap_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, portal_bootstrap_js_start, portal_bootstrap_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_wifi_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_wifi_js_start, tab_wifi_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_ai_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_ai_js_start, tab_ai_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_telegram_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_telegram_js_start, tab_telegram_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_stocks_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_stocks_js_start, tab_stocks_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_at_time_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_at_time_js_start, tab_at_time_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
}

static esp_err_t portal_tab_interval_js_get_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, tab_interval_js_start, tab_interval_js_end,
                               "application/javascript; charset=utf-8", CACHE_ASSET_PUBLIC);
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
        free(body);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    get_form_value(body, "ssid", conn_req->ssid, sizeof(conn_req->ssid));
    get_form_value(body, "password", conn_req->password, sizeof(conn_req->password));

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
                strlcpy(conn_req->password, password, sizeof(conn_req->password));
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
    if (s_httpd) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 40;
    config.max_open_sockets = 3;
    config.backlog_conn = 4;
    config.lru_purge_enable = true;
    config.keep_alive_enable = false;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    /* Portal handlers include JSON assembly and multiple local buffers; keep headroom. */
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&s_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Portal HTTP server 啟動失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = portal_root_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t wifi_page_uri = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = portal_wifi_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_page_uri = {
        .uri = "/ai",
        .method = HTTP_GET,
        .handler = portal_ai_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_page_uri = {
        .uri = "/telegram",
        .method = HTTP_GET,
        .handler = portal_telegram_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t stocks_page_uri = {
        .uri = "/stocks",
        .method = HTTP_GET,
        .handler = portal_stocks_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t at_time_page_uri = {
        .uri = "/at_time",
        .method = HTTP_GET,
        .handler = portal_at_time_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t interval_page_uri = {
        .uri = "/interval",
        .method = HTTP_GET,
        .handler = portal_interval_page_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t wifi_post_uri = {
        .uri = "/wifi",
        .method = HTTP_POST,
        .handler = portal_wifi_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t css_uri = {
        .uri = "/assets/portal.css",
        .method = HTTP_GET,
        .handler = portal_css_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t bootstrap_js_uri = {
        .uri = "/assets/portal_bootstrap.js",
        .method = HTTP_GET,
        .handler = portal_bootstrap_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_wifi_js_uri = {
        .uri = "/assets/tab_wifi.js",
        .method = HTTP_GET,
        .handler = portal_tab_wifi_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_ai_js_uri = {
        .uri = "/assets/tab_ai.js",
        .method = HTTP_GET,
        .handler = portal_tab_ai_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_telegram_js_uri = {
        .uri = "/assets/tab_telegram.js",
        .method = HTTP_GET,
        .handler = portal_tab_telegram_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_stocks_js_uri = {
        .uri = "/assets/tab_stocks.js",
        .method = HTTP_GET,
        .handler = portal_tab_stocks_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_at_time_js_uri = {
        .uri = "/assets/tab_at_time.js",
        .method = HTTP_GET,
        .handler = portal_tab_at_time_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t tab_interval_js_uri = {
        .uri = "/assets/tab_interval.js",
        .method = HTTP_GET,
        .handler = portal_tab_interval_js_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t scan_uri = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = portal_scan_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_get_uri = {
        .uri = "/api/ai",
        .method = HTTP_GET,
        .handler = portal_ai_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_post_uri = {
        .uri = "/api/ai",
        .method = HTTP_POST,
        .handler = portal_ai_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_test_post_uri = {
        .uri = "/api/ai/test",
        .method = HTTP_POST,
        .handler = portal_ai_test_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_get_uri = {
        .uri = "/api/telegram",
        .method = HTTP_GET,
        .handler = portal_telegram_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_post_uri = {
        .uri = "/api/telegram",
        .method = HTTP_POST,
        .handler = portal_telegram_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_test_post_uri = {
        .uri = "/api/telegram/test",
        .method = HTTP_POST,
        .handler = portal_telegram_test_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_check_post_uri = {
        .uri = "/api/telegram/check",
        .method = HTTP_POST,
        .handler = portal_telegram_check_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t telegram_chats_get_uri = {
        .uri = "/api/telegram/chats",
        .method = HTTP_GET,
        .handler = portal_telegram_chats_get_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(s_httpd, &root_uri);
    httpd_register_uri_handler(s_httpd, &wifi_page_uri);
    httpd_register_uri_handler(s_httpd, &ai_page_uri);
    httpd_register_uri_handler(s_httpd, &telegram_page_uri);
    httpd_register_uri_handler(s_httpd, &stocks_page_uri);
    httpd_register_uri_handler(s_httpd, &at_time_page_uri);
    httpd_register_uri_handler(s_httpd, &interval_page_uri);
    httpd_register_uri_handler(s_httpd, &wifi_post_uri);
    httpd_register_uri_handler(s_httpd, &css_uri);
    httpd_register_uri_handler(s_httpd, &bootstrap_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_wifi_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_ai_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_telegram_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_stocks_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_at_time_js_uri);
    httpd_register_uri_handler(s_httpd, &tab_interval_js_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &ai_get_uri);
    httpd_register_uri_handler(s_httpd, &ai_post_uri);
    httpd_register_uri_handler(s_httpd, &ai_test_post_uri);
    httpd_register_uri_handler(s_httpd, &telegram_get_uri);
    httpd_register_uri_handler(s_httpd, &telegram_post_uri);
    httpd_register_uri_handler(s_httpd, &telegram_test_post_uri);
    httpd_register_uri_handler(s_httpd, &telegram_check_post_uri);
    httpd_register_uri_handler(s_httpd, &telegram_chats_get_uri);
    stock_admin_service_register_handlers(s_httpd);
    at_time_admin_service_register_handlers(s_httpd);

    return ESP_OK;
}

static void stop_portal_http_server(void)
{
    if (!s_httpd) {
        return;
    }
    httpd_stop(s_httpd);
    s_httpd = NULL;
}

esp_err_t portal_backend_start(void)
{
    if (!wifi_manager_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_portal_active) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[%u ms] portal_start begin", (unsigned)esp_log_timestamp());

    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "建立 AP netif 失敗");
            return ESP_FAIL;
        }
    }

    wifi_config_t ap_cfg = {0};
    strlcpy((char *)ap_cfg.ap.ssid, s_portal_ap_ssid, sizeof(ap_cfg.ap.ssid));
    strlcpy((char *)ap_cfg.ap.password, s_portal_ap_password, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len = strlen((const char *)ap_cfg.ap.ssid);
    ap_cfg.ap.channel = WIFI_PORTAL_AP_CHANNEL;
    ap_cfg.ap.max_connection = WIFI_PORTAL_MAX_STA;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    /* Keep STA online while portal is active so LAN IP access still works. */
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切換 APSTA 模式失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "設定 AP 參數失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = start_portal_http_server();
    if (ret != ESP_OK) {
        return ret;
    }

    s_portal_active = true;
    ESP_LOGI(TAG, "Portal 已啟動 AP=%s URL=%s", s_portal_ap_ssid, s_portal_url);
    return ESP_OK;
}

esp_err_t portal_backend_stop(void)
{
    if (!s_portal_active) {
        return ESP_OK;
    }

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切換 STA 模式失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    stop_portal_http_server();
    s_portal_active = false;
    ESP_LOGI(TAG, "Portal 已停止");
    return ESP_OK;
}

bool portal_backend_is_active(void)
{
    return s_portal_active;
}

const char *portal_backend_get_ap_ssid(void)
{
    return s_portal_ap_ssid;
}

const char *portal_backend_get_ap_password(void)
{
    return s_portal_ap_password;
}

const char *portal_backend_get_url(void)
{
    return s_portal_url;
}

const char *portal_backend_get_ap_ip(void)
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

void portal_backend_set_stock_list_changed_callback(stock_list_changed_cb_t cb)
{
    stock_admin_service_set_stock_list_changed_callback(cb);
}
