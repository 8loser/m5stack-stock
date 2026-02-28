#include "wifi_manager.h"
#include "storage.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <netdb.h>

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           5

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t       s_state            = WIFI_STATE_DISCONNECTED;
static char               s_ip_str[20]       = "0.0.0.0";
static int                s_retry_count      = 0;
static wifi_state_cb_t    s_callback         = NULL;
static bool               s_initialized      = false;

static void notify_state(wifi_state_t new_state)
{
    s_state = new_state;
    if (s_callback) s_callback(new_state, s_ip_str);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        notify_state(WIFI_STATE_CONNECTING);

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

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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
    return ESP_FAIL;
}

esp_err_t wifi_manager_connect_saved(void)
{
    char ssid[64] = {0}, password[128] = {0};
    if (!storage_wifi_has_saved()) {
        ESP_LOGI(TAG, "無已儲存的 WiFi 設定");
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
