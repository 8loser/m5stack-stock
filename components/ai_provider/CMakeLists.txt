idf_component_register(
    SRCS
        "ai_provider.c"
        "remote_prompt.c"
        "providers/gemini.c"
        "providers/claude.c"
        "providers/openai.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_http_client
        mbedtls
        json
        storage
        twse_client
        log
        freertos
)
