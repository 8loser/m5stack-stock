#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t portal_service_start(void);
esp_err_t portal_service_stop(void);
bool portal_service_is_active(void);
const char *portal_service_get_ap_ssid(void);
const char *portal_service_get_ap_password(void);
const char *portal_service_get_url(void);
const char *portal_service_get_ap_ip(void);
