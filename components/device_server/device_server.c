#include "device_server.h"
#include "wifi_service.h"
#include "portal_service.h"
#include "stock_admin_service.h"

esp_err_t device_server_init(void)
{
    return wifi_service_init();
}

esp_err_t device_server_connect(const char *ssid, const char *password)
{
    return wifi_service_connect(ssid, password);
}

esp_err_t device_server_connect_any_saved(void)
{
    return wifi_service_connect_any_saved();
}

esp_err_t device_server_connect_saved(void)
{
    return wifi_service_connect_saved();
}

esp_err_t device_server_disconnect(void)
{
    return wifi_service_disconnect();
}

wifi_state_t device_server_get_state(void)
{
    return wifi_service_get_state();
}

bool device_server_is_connected(void)
{
    return wifi_service_is_connected();
}

const char *device_server_get_ip(void)
{
    return wifi_service_get_ip();
}

const char *device_server_get_connected_ssid(void)
{
    return wifi_service_get_connected_ssid();
}

esp_err_t device_server_start_provisioning_portal(void)
{
    return portal_service_start();
}

esp_err_t device_server_stop_provisioning_portal(void)
{
    return portal_service_stop();
}

bool device_server_is_provisioning_portal_active(void)
{
    return portal_service_is_active();
}

const char *device_server_get_provisioning_ap_ssid(void)
{
    return portal_service_get_ap_ssid();
}

const char *device_server_get_provisioning_ap_password(void)
{
    return portal_service_get_ap_password();
}

const char *device_server_get_provisioning_url(void)
{
    return portal_service_get_url();
}

const char *device_server_get_provisioning_ap_ip(void)
{
    return portal_service_get_ap_ip();
}

esp_err_t device_server_scan(wifi_ap_info_t *results, uint16_t *count, uint16_t max_count)
{
    return wifi_service_scan(results, count, max_count);
}

void device_server_set_callback(wifi_state_cb_t cb)
{
    wifi_service_set_callback(cb);
}

void device_server_set_stock_list_changed_callback(stock_list_changed_cb_t cb)
{
    stock_admin_service_set_stock_list_changed_callback(cb);
}
