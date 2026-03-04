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
typedef void (*stock_list_changed_cb_t)(uint8_t count);

esp_err_t   network_portal_init(void);
esp_err_t   network_portal_connect(const char *ssid, const char *password);
esp_err_t   network_portal_connect_any_saved(void);
esp_err_t   network_portal_connect_saved(void);
esp_err_t   network_portal_disconnect(void);
wifi_state_t network_portal_get_state(void);
bool         network_portal_is_connected(void);
const char  *network_portal_get_ip(void);
const char  *network_portal_get_connected_ssid(void);

/* 手機配網入口（SoftAP + HTTP） */
esp_err_t   network_portal_start_provisioning_portal(void);
esp_err_t   network_portal_stop_provisioning_portal(void);
bool        network_portal_is_provisioning_portal_active(void);
const char *network_portal_get_provisioning_ap_ssid(void);
const char *network_portal_get_provisioning_ap_password(void);
const char *network_portal_get_provisioning_url(void);
const char *network_portal_get_provisioning_ap_ip(void);

/* AP 掃描 */
esp_err_t   network_portal_scan(wifi_ap_info_t *results, uint16_t *count,
                               uint16_t max_count);

/* 狀態回調 */
void        network_portal_set_callback(wifi_state_cb_t cb);
void        network_portal_set_stock_list_changed_callback(stock_list_changed_cb_t cb);
