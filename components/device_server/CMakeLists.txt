idf_component_register(
    SRCS "device_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif esp_http_server storage scheduler twse_client json log freertos
)
