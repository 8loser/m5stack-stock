idf_component_register(
    SRCS
        "device_server.c"
        "wifi_service.c"
        "portal_service.c"
        "stock_admin_service.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "portal/index.html"
    REQUIRES esp_wifi esp_event esp_netif esp_http_server esp_http_client mbedtls storage scheduler twse_client ai_provider json log freertos
)
