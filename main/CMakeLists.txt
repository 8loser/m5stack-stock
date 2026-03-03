idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        board
        app_core
        storage
        device_server
        twse_client
        scheduler
        ui
        nvs_flash
        esp_wifi
        freertos
)
