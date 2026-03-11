idf_component_register(
    SRCS
        "board.c"
        "axp192.c"
        "ili9342c.c"
        "ft6336u.c"
        "rtc_bm8563.c"
        "vibration.c"
        "audio.c"
        "alert_feedback.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_lcd
        esp_timer
        freertos
        log
)

# Work around xtensa gcc ICE in audio_beep with -O2; keep this file at -O1.
set_source_files_properties("audio.c" PROPERTIES COMPILE_OPTIONS "-O1")
