#include "network_portal.h"

esp_err_t network_portal_init(void)
{
    return wifi_manager_init();
}

esp_err_t network_portal_connect(const char *ssid, const char *password)
{
    return wifi_manager_connect(ssid, password);
}

esp_err_t network_portal_connect_any_saved(void)
{
    return wifi_manager_connect_any_saved();
}

esp_err_t network_portal_connect_saved(void)
{
    return wifi_manager_connect_saved();
}

esp_err_t network_portal_disconnect(void)
{
    return wifi_manager_disconnect();
}

wifi_state_t network_portal_get_state(void)
{
    return wifi_manager_get_state();
}

bool network_portal_is_connected(void)
{
    return wifi_manager_is_connected();
}

const char *network_portal_get_ip(void)
{
    return wifi_manager_get_ip();
}

const char *network_portal_get_connected_ssid(void)
{
    return wifi_manager_get_connected_ssid();
}

esp_err_t network_portal_start_provisioning_portal(void)
{
    return portal_backend_start();
}

esp_err_t network_portal_stop_provisioning_portal(void)
{
    return portal_backend_stop();
}

bool network_portal_is_provisioning_portal_active(void)
{
    return portal_backend_is_active();
}

bool network_portal_is_portal_sta_mode(void)
{
    return portal_backend_is_sta_mode();
}

const char *network_portal_get_provisioning_ap_ssid(void)
{
    return portal_backend_get_ap_ssid();
}

const char *network_portal_get_provisioning_ap_password(void)
{
    return portal_backend_get_ap_password();
}

const char *network_portal_get_provisioning_url(void)
{
    return portal_backend_get_url();
}

const char *network_portal_get_provisioning_ap_ip(void)
{
    return portal_backend_get_ap_ip();
}

esp_err_t network_portal_scan(wifi_ap_info_t *results, uint16_t *count, uint16_t max_count)
{
    return wifi_manager_scan(results, count, max_count);
}

void network_portal_set_callback(wifi_state_cb_t cb)
{
    wifi_manager_set_callback(cb);
}

void network_portal_set_stock_list_changed_callback(stock_list_changed_cb_t cb)
{
    portal_backend_set_stock_list_changed_callback(cb);
}

void network_portal_cache_quote(const stock_quote_t *quote)
{
    portal_backend_cache_quote(quote);
}
