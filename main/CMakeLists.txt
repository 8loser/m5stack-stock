idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        board
        app_core
        storage
        device_server
        telegram_bot
        twse_client
        scheduler
        ui
        nvs_flash
        esp_wifi
        freertos
)
