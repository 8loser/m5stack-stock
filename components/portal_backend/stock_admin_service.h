#pragma once

#include "portal_backend.h"
#include "twse_models.h"
#include "esp_err.h"
#include "esp_http_server.h"

void stock_admin_service_set_stock_list_changed_callback(stock_list_changed_cb_t cb);
void stock_admin_service_cache_quote(const stock_quote_t *quote);
esp_err_t stock_admin_service_register_handlers(httpd_handle_t httpd);
