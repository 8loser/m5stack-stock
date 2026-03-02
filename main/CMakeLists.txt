idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        board
        storage
        wifi_manager
        twse_client
        scheduler
        ui
        nvs_flash
        esp_wifi
        freertos
)
