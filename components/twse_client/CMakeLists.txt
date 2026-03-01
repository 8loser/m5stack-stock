idf_component_register(
    SRCS "twse_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client mbedtls json log freertos
)
