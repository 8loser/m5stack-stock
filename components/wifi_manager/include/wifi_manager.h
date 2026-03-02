#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
} wifi_state_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
} wifi_ap_info_t;

typedef void (*wifi_state_cb_t)(wifi_state_t state, const char *ip_str);

esp_err_t   wifi_manager_init(void);
esp_err_t   wifi_manager_connect(const char *ssid, const char *password);
esp_err_t   wifi_manager_connect_any_saved(void);
esp_err_t   wifi_manager_connect_saved(void);
esp_err_t   wifi_manager_disconnect(void);
wifi_state_t wifi_manager_get_state(void);
bool         wifi_manager_is_connected(void);
const char  *wifi_manager_get_ip(void);
const char  *wifi_manager_get_connected_ssid(void);

/* 手機配網入口（SoftAP + HTTP） */
esp_err_t   wifi_manager_start_provisioning_portal(void);
esp_err_t   wifi_manager_stop_provisioning_portal(void);
bool        wifi_manager_is_provisioning_portal_active(void);
const char *wifi_manager_get_provisioning_ap_ssid(void);
const char *wifi_manager_get_provisioning_ap_password(void);
const char *wifi_manager_get_provisioning_url(void);

/* AP 掃描 */
esp_err_t   wifi_manager_scan(wifi_ap_info_t *results, uint16_t *count,
                               uint16_t max_count);

/* 狀態回調 */
void        wifi_manager_set_callback(wifi_state_cb_t cb);
