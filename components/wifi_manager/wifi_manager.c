#include "wifi_manager.h"
#include "storage.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           5

static const char *TAG = "wifi_manager";

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static char s_ip_str[20] = "0.0.0.0";
static char s_connected_ssid[33] = "";
static int s_retry_count = 0;
static wifi_state_cb_t s_callback = NULL;
static bool s_initialized = false;

static void notify_state(wifi_state_t new_state)
{
    s_state = new_state;
    if (s_callback) {
        s_callback(new_state, s_ip_str);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        notify_state(WIFI_STATE_DISCONNECTED);
        ESP_LOGI(TAG, "[%u ms] STA_START", (unsigned)esp_log_timestamp());
        return;
    }

    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc = (const wifi_event_sta_disconnected_t *)event_data;
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);

        ESP_LOGW(TAG, "[%u ms] STA_DISCONNECTED reason=%d mode=%d retry=%d/%d",
                 (unsigned)esp_log_timestamp(),
                 disc ? (int)disc->reason : -1,
                 (int)mode,
                 s_retry_count,
                 MAX_RETRY);

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
        return;
    }

    if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_ap_record_t ap_info = {0};

        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
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

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        return ESP_ERR_NO_MEM;
    }

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
    ESP_LOGI(TAG, "WiFi service 初始化完成");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_cfg = {0};

    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    s_retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    esp_wifi_disconnect();
    esp_wifi_connect();
    notify_state(WIFI_STATE_CONNECTING);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        storage_wifi_add_ap(ssid, password);
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        notify_state(WIFI_STATE_FAILED);
    } else {
        notify_state(WIFI_STATE_DISCONNECTED);
        ESP_LOGW(TAG, "WiFi 連線逾時");
    }
    return ESP_FAIL;
}

esp_err_t wifi_manager_connect_any_saved(void)
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
        if (wifi_manager_connect(ssid, password) == ESP_OK) {
            return ESP_OK;
        }
    }

    notify_state(WIFI_STATE_FAILED);
    return ESP_FAIL;
}

esp_err_t wifi_manager_connect_saved(void)
{
    return wifi_manager_connect_any_saved();
}

esp_err_t wifi_manager_disconnect(void)
{
    s_retry_count = MAX_RETRY;
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

const char *wifi_manager_get_connected_ssid(void)
{
    return s_connected_ssid;
}

void wifi_manager_set_callback(wifi_state_cb_t cb)
{
    s_callback = cb;
}

esp_err_t wifi_manager_scan(wifi_ap_info_t *results, uint16_t *count, uint16_t max_count)
{
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t ap_count = max_count;
    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!ap_records) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret == ESP_OK) {
        *count = (ap_count < max_count) ? ap_count : max_count;
        for (uint16_t i = 0; i < *count; i++) {
            strncpy(results[i].ssid, (char *)ap_records[i].ssid, 32);
            results[i].ssid[32] = '\0';
            results[i].rssi = ap_records[i].rssi;
        }
    }

    free(ap_records);
    return ret;
}

bool wifi_manager_is_initialized(void)
{
    return s_initialized;
}
