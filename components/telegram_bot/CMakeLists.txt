idf_component_register(
    SRCS "telegram_bot.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_wifi storage device_server twse_client json log freertos mbedtls
)
