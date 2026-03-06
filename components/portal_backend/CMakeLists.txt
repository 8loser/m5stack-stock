idf_component_register(
    SRCS
        "portal_backend.c"
        "stock_admin_service.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES
        "portal/wifi.html"
        "portal/ai.html"
        "portal/telegram.html"
        "portal/stocks.html"
        "portal/at_time.html"
        "portal/interval.html"
        "portal/portal.css"
        "portal/portal_bootstrap.js"
        "portal/tab_wifi.js"
        "portal/tab_ai.js"
        "portal/tab_telegram.js"
        "portal/tab_stocks.js"
        "portal/tab_at_time.js"
        "portal/tab_interval.js"
    REQUIRES esp_wifi esp_netif esp_http_server esp_http_client mbedtls storage scheduler_service twse_client ai_provider json log freertos wifi_manager
)
