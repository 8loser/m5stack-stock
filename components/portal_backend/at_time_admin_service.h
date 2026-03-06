#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t at_time_admin_service_register_handlers(httpd_handle_t httpd);
