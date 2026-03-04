#pragma once

#include "network_portal.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t wifi_service_init(void);
esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_connect_any_saved(void);
esp_err_t wifi_service_connect_saved(void);
esp_err_t wifi_service_disconnect(void);

wifi_state_t wifi_service_get_state(void);
bool wifi_service_is_connected(void);
const char *wifi_service_get_ip(void);
const char *wifi_service_get_connected_ssid(void);
void wifi_service_set_callback(wifi_state_cb_t cb);

esp_err_t wifi_service_scan(wifi_ap_info_t *results, uint16_t *count, uint16_t max_count);
bool wifi_service_is_initialized(void);
