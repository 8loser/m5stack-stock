idf_component_register(
    SRCS
        "scheduler.c"
        "sleep_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        esp_wifi
        esp_hw_support
        storage
        twse_client
        board
        log
        freertos
        lwip
)
