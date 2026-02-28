idf_component_register(
    SRCS
        "board.c"
        "axp192.c"
        "ili9342c.c"
        "ft6336u.c"
        "rtc_bm8563.c"
        "vibration.c"
        "audio.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_lcd
        esp_timer
        freertos
        log
)
