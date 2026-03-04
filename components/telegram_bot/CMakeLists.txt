idf_component_register(
    SRCS "telegram_bot.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_wifi storage network_portal twse_client json log freertos mbedtls
)
