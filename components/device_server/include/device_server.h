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

esp_err_t   device_server_init(void);
esp_err_t   device_server_connect(const char *ssid, const char *password);
esp_err_t   device_server_connect_any_saved(void);
esp_err_t   device_server_connect_saved(void);
esp_err_t   device_server_disconnect(void);
wifi_state_t device_server_get_state(void);
bool         device_server_is_connected(void);
const char  *device_server_get_ip(void);
const char  *device_server_get_connected_ssid(void);

/* 手機配網入口（SoftAP + HTTP） */
esp_err_t   device_server_start_provisioning_portal(void);
esp_err_t   device_server_stop_provisioning_portal(void);
bool        device_server_is_provisioning_portal_active(void);
const char *device_server_get_provisioning_ap_ssid(void);
const char *device_server_get_provisioning_ap_password(void);
const char *device_server_get_provisioning_url(void);

/* AP 掃描 */
esp_err_t   device_server_scan(wifi_ap_info_t *results, uint16_t *count,
                               uint16_t max_count);

/* 狀態回調 */
void        device_server_set_callback(wifi_state_cb_t cb);
