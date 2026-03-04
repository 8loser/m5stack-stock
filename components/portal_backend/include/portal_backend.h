#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*stock_list_changed_cb_t)(uint8_t count);

esp_err_t portal_backend_start(void);
esp_err_t portal_backend_stop(void);
bool portal_backend_is_active(void);
const char *portal_backend_get_ap_ssid(void);
const char *portal_backend_get_ap_password(void);
const char *portal_backend_get_url(void);
const char *portal_backend_get_ap_ip(void);

void portal_backend_set_stock_list_changed_callback(stock_list_changed_cb_t cb);
