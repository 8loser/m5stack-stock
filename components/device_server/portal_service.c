#include "portal_service.h"
#include "stock_admin_service.h"
#include "wifi_service.h"
#include "device_server.h"
#include "storage.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTAL_BODY_MAX_LEN 4096
#define PORTAL_SCAN_MAX_APS 20

enum {
    AI_PROVIDER_GEMINI = 0,
    AI_PROVIDER_CLAUDE = 1,
    AI_PROVIDER_OPENAI = 2,
};

typedef struct {
    char ssid[33];
    char password[65];
} wifi_connect_req_t;

static const char *TAG = "portal_service";
static esp_netif_t *s_ap_netif = NULL;
static httpd_handle_t s_httpd = NULL;
static bool s_portal_active = false;
static bool s_connecting_busy = false;

static char s_portal_ap_ssid[33] = WIFI_PORTAL_AP_SSID;
static char s_portal_ap_password[65] = WIFI_PORTAL_AP_PASSWORD;
static const char *s_portal_url = WIFI_PORTAL_URL;

extern const uint8_t portal_html_start[] asm("_binary_index_html_start");
extern const uint8_t portal_html_end[] asm("_binary_index_html_end");

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
            esp_err_t stop_ret = portal_service_stop();
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
    wifi_ap_info_t ap_infos[PORTAL_SCAN_MAX_APS] = {0};
    uint16_t count = PORTAL_SCAN_MAX_APS;

    esp_err_t ret = wifi_service_scan(ap_infos, &count, PORTAL_SCAN_MAX_APS);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ret;
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
        if (n > 0) {
            pos += (size_t)n;
        }
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

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
    char e_gemini[300] = {0};
    char e_claude[300] = {0};
    char e_openai[300] = {0};

    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key, sizeof(gemini_key));
    if (gemini_key[0] == '\0') {
        storage_ai_load_key(gemini_key, sizeof(gemini_key));
    }
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key, sizeof(claude_key));
    storage_ai_load_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key, sizeof(openai_key));

    json_escape(e_gemini, sizeof(e_gemini), gemini_key);
    json_escape(e_claude, sizeof(e_claude), claude_key);
    json_escape(e_openai, sizeof(e_openai), openai_key);

    char *json = malloc(1200);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }

    int n = snprintf(json, 1200,
                     "{\"gemini_key\":\"%s\",\"claude_key\":\"%s\","
                     "\"openai_key\":\"%s\"}",
                     e_gemini, e_claude, e_openai);

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

    get_form_value(body, "gemini_key", gemini_key, sizeof(gemini_key));
    get_form_value(body, "claude_key", claude_key, sizeof(claude_key));
    get_form_value(body, "openai_key", openai_key, sizeof(openai_key));
    free(body);

    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req,
                              "<html><body><h3>AI 設定已儲存</h3>"
                              "<p>可返回上一頁繼續調整。</p><a href='/'>Back</a></body></html>");
}

static esp_err_t portal_index_get_handler(httpd_req_t *req)
{
    size_t html_len = (size_t)(portal_html_end - portal_html_start);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)portal_html_start, (ssize_t)html_len);
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
    if (s_httpd) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 12;
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&s_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Portal HTTP server 啟動失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = portal_index_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t wifi_uri = {
        .uri = "/wifi",
        .method = HTTP_POST,
        .handler = portal_wifi_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = portal_scan_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_get_uri = {
        .uri = "/ai",
        .method = HTTP_GET,
        .handler = portal_ai_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ai_post_uri = {
        .uri = "/ai",
        .method = HTTP_POST,
        .handler = portal_ai_post_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(s_httpd, &index_uri);
    httpd_register_uri_handler(s_httpd, &wifi_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &ai_get_uri);
    httpd_register_uri_handler(s_httpd, &ai_post_uri);
    stock_admin_service_register_handlers(s_httpd);

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

esp_err_t portal_service_start(void)
{
    if (!wifi_service_is_initialized()) {
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
    strncpy((char *)ap_cfg.ap.ssid, s_portal_ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char *)ap_cfg.ap.password, s_portal_ap_password, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.ssid_len = strlen(s_portal_ap_ssid);
    ap_cfg.ap.channel = WIFI_PORTAL_AP_CHANNEL;
    ap_cfg.ap.max_connection = WIFI_PORTAL_MAX_STA;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

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

esp_err_t portal_service_stop(void)
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

bool portal_service_is_active(void)
{
    return s_portal_active;
}

const char *portal_service_get_ap_ssid(void)
{
    return s_portal_ap_ssid;
}

const char *portal_service_get_ap_password(void)
{
    return s_portal_ap_password;
}

const char *portal_service_get_url(void)
{
    return s_portal_url;
}

const char *portal_service_get_ap_ip(void)
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
