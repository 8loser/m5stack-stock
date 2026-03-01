#include "wifi_manager.h"
#include "storage.h"
#include "ai_provider.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           5
#define PORTAL_BODY_MAX_LEN 4096
#define PORTAL_SCAN_MAX_APS 20

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t       s_state            = WIFI_STATE_DISCONNECTED;
static char               s_ip_str[20]       = "0.0.0.0";
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
    esp_err_t ret = wifi_manager_connect(req->ssid, req->password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Portal 配網成功");
        if (s_portal_active) {
            esp_err_t stop_ret = wifi_manager_stop_provisioning_portal();
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
    if (!req || req->content_len <= 0 || req->content_len > PORTAL_BODY_MAX_LEN) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid request");
        return ESP_FAIL;
    }

    char body[PORTAL_BODY_MAX_LEN + 1] = {0};
    int remaining = req->content_len;
    int offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, body + offset, remaining);
        if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        offset += recv_len;
        remaining -= recv_len;
    }
    body[offset] = '\0';

    char gemini_key[128] = {0};
    char claude_key[128] = {0};
    char openai_key[128] = {0};
    char prompt_template[1024] = {0};

    get_form_value(body, "gemini_key", gemini_key, sizeof(gemini_key));
    get_form_value(body, "claude_key", claude_key, sizeof(claude_key));
    get_form_value(body, "openai_key", openai_key, sizeof(openai_key));
    get_form_value(body, "prompt_template", prompt_template, sizeof(prompt_template));

    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_GEMINI, gemini_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_CLAUDE, claude_key);
    storage_ai_save_provider_key((uint8_t)AI_PROVIDER_OPENAI, openai_key);
    storage_ai_save_prompt_template(prompt_template);

    ai_provider_reload_local_config();

    if (gemini_key[0] != '\0') {
        ai_provider_set_type(AI_PROVIDER_GEMINI);
    } else if (claude_key[0] != '\0') {
        ai_provider_set_type(AI_PROVIDER_CLAUDE);
    } else if (openai_key[0] != '\0') {
        ai_provider_set_type(AI_PROVIDER_OPENAI);
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req,
        "<html><body><h3>AI 設定已儲存</h3>"
        "<p>可返回上一頁繼續調整。</p><a href='/'>Back</a></body></html>");
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
        ".ok{color:#0a6;font-size:13px;}</style>"
        "</head><body><h2>M5Stack Core2 Portal</h2>"
        "<div class='grid'>"
        "<div class='card'>"
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
        "</form></div>"
        "<div class='card'>"
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
        "</div>"
        "<script>"
        "fetch('/scan').then(function(r){return r.json();}).then(function(a){"
        "var s=document.getElementById('ss');"
        "s.options.length=0;"
        "s.add(new Option('-- Select AP --',''));"
        "a.forEach(function(x){"
        "var o=new Option();"
        "o.textContent=x.ssid+'  ('+x.rssi+'dBm)';"
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
        "function chk(s){"
        "document.getElementById('m').style.display="
        "(s.value=='__manual__')?'block':'none';}"
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

    char body[PORTAL_BODY_MAX_LEN + 1] = {0};
    int remaining = req->content_len;
    int offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, body + offset, remaining);
        if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        offset += recv_len;
        remaining -= recv_len;
    }

    body[offset] = '\0';

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

    if (conn_req->ssid[0] == '\0') {
        free(conn_req);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
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
    config.max_uri_handlers = 10;

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

    httpd_register_uri_handler(s_httpd, &index_uri);
    httpd_register_uri_handler(s_httpd, &wifi_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &ai_get_uri);
    httpd_register_uri_handler(s_httpd, &ai_post_uri);

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

    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
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
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR,
                 IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        notify_state(WIFI_STATE_CONNECTED);
        ESP_LOGI(TAG, "WiFi 已連線 IP=%s", s_ip_str);
    }
}

esp_err_t wifi_manager_init(void)
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
    ESP_LOGI(TAG, "WiFi Manager 初始化完成");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
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
        storage_wifi_save(ssid, password);
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

esp_err_t wifi_manager_connect_saved(void)
{
    char ssid[64] = {0}, password[128] = {0};
    if (!storage_wifi_has_saved()) {
        ESP_LOGI(TAG, "無已儲存的 WiFi 設定");
        notify_state(WIFI_STATE_DISCONNECTED);
        return ESP_ERR_NOT_FOUND;
    }
    storage_wifi_load(ssid, sizeof(ssid), password, sizeof(password));
    return wifi_manager_connect(ssid, password);
}

esp_err_t wifi_manager_disconnect(void)
{
    s_retry_count = MAX_RETRY;  /* 停止自動重試 */
    esp_wifi_disconnect();
    notify_state(WIFI_STATE_DISCONNECTED);
    return ESP_OK;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_state;
}

bool wifi_manager_is_connected(void)
{
    return (s_state == WIFI_STATE_CONNECTED);
}

const char *wifi_manager_get_ip(void)
{
    return s_ip_str;
}

void wifi_manager_set_callback(wifi_state_cb_t cb)
{
    s_callback = cb;
}

esp_err_t wifi_manager_start_provisioning_portal(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (s_portal_active) return ESP_OK;

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    esp_err_t ret = start_portal_http_server();
    if (ret != ESP_OK) return ret;

    s_portal_active = true;

    ESP_LOGI(TAG, "Portal 已啟動 AP=%s URL=%s", s_portal_ap_ssid, s_portal_url);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_provisioning_portal(void)
{
    if (!s_portal_active) return ESP_OK;

    stop_portal_http_server();
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切換 STA 模式失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    s_portal_active = false;
    ESP_LOGI(TAG, "Portal 已停止");
    return ESP_OK;
}

bool wifi_manager_is_provisioning_portal_active(void)
{
    return s_portal_active;
}

const char *wifi_manager_get_provisioning_ap_ssid(void)
{
    return s_portal_ap_ssid;
}

const char *wifi_manager_get_provisioning_ap_password(void)
{
    return s_portal_ap_password;
}

const char *wifi_manager_get_provisioning_url(void)
{
    return s_portal_url;
}

esp_err_t wifi_manager_scan(wifi_ap_info_t *results, uint16_t *count,
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
