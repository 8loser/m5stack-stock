#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "app_event_bus.h"
#include "board.h"
#include "storage.h"
#include "device_server.h"
#include "twse_client.h"
#include "scheduler.h"
#include "ui_manager.h"

static const char *TAG = "main";

static void on_scheduler_tick_event(const app_event_t *evt, void *ctx)
{
    (void)evt;
    (void)ctx;
    ui_manager_heartbeat_feed_scheduler();
}

static void on_quote_fetch_round_event(const app_event_t *evt, void *ctx)
{
    (void)ctx;
    const bool force_fetch = evt->data.quote_fetch_round.force_fetch;

    switch (evt->data.quote_fetch_round.reason) {
    case APP_QUOTE_FETCH_REASON_FETCH_DONE:
        ui_manager_log_stock(LOG_LEVEL_INFO,
                             "Fetch done f=%d t=%u p=%u e=%u i=%u",
                             force_fetch ? 1 : 0,
                             (unsigned)evt->data.quote_fetch_round.total,
                             (unsigned)evt->data.quote_fetch_round.pushed,
                             (unsigned)evt->data.quote_fetch_round.skipped_empty,
                             (unsigned)evt->data.quote_fetch_round.skipped_invalid);
        break;
    case APP_QUOTE_FETCH_REASON_FETCH_FAILED:
        ui_manager_log_stock(LOG_LEVEL_WARN,
                             "Fetch fail f=%d err=%s t=%u",
                             force_fetch ? 1 : 0,
                             esp_err_to_name(evt->data.quote_fetch_round.fetch_err),
                             (unsigned)evt->data.quote_fetch_round.total);
        break;
    case APP_QUOTE_FETCH_REASON_WIFI_DISCONNECTED:
        ui_manager_log_stock(LOG_LEVEL_WARN, "Fetch skip no-wifi");
        break;
    case APP_QUOTE_FETCH_REASON_EMPTY_LIST:
        ui_manager_log_stock(LOG_LEVEL_WARN, "Fetch skip empty-list");
        break;
    case APP_QUOTE_FETCH_REASON_MARKET_CLOSED:
        ui_manager_log_stock(LOG_LEVEL_INFO, "Fetch skip market-closed");
        break;
    default:
        break;
    }
}

/* WiFi 狀態回調：橋接 device_server → ui_manager */
static void on_wifi_state(wifi_state_t state, const char *ip)
{
    ui_manager_update_wifi_state((int)state, ip);
    if (state == WIFI_STATE_CONNECTED) {
        const char *ssid = device_server_get_connected_ssid();
        ui_manager_log_wifi(LOG_LEVEL_INFO, "Connected: %s (%s)",
                            (ssid && ssid[0] != '\0') ? ssid : "unknown",
                            (ip != NULL) ? ip : "");
    } else {
        ui_manager_log_wifi(LOG_LEVEL_WARN, "Disconnected");
    }
}

static void on_stock_list_changed(uint8_t count)
{
    stock_list_t stocks = {0};
    if (storage_stocks_load(&stocks) == ESP_OK) {
        ui_manager_set_dashboard_symbols((const char (*)[8])stocks.symbols, stocks.count);
        ui_manager_log_stock(LOG_LEVEL_INFO, "Stock list changed, count=%u", (unsigned)stocks.count);
        return;
    }

    ui_manager_set_dashboard_card_count(count);
    ui_manager_log_stock(LOG_LEVEL_WARN, "Stock list changed, but load failed. use count=%u", (unsigned)count);
}

/* 全域共用資源 */
QueueHandle_t   g_quote_queue      = NULL;
SemaphoreHandle_t g_ui_mutex       = NULL;

static void init_global_resources(void)
{
    g_quote_queue     = xQueueCreate(MAX_STOCK_COUNT, sizeof(stock_quote_t));
    /* 遞迴 mutex：LVGL task 持有 mutex 時，事件回調仍可安全呼叫 ui_manager_* */
    g_ui_mutex        = xSemaphoreCreateRecursiveMutex();

    if (!g_quote_queue || !g_ui_mutex) {
        ESP_LOGE(TAG, "FreeRTOS 資源建立失敗");
        esp_restart();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== M5Stack Core2 台股監測 啟動 ===");
    ESP_LOGI(TAG, "reset reason=%d", (int)esp_reset_reason());

    /* NVS 初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 格式化中...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 共用佇列 / mutex */
    init_global_resources();
    ESP_ERROR_CHECK(app_event_bus_init());
    ESP_ERROR_CHECK(app_event_subscribe(APP_EVENT_SCHEDULER_TICK, on_scheduler_tick_event, NULL));
    ESP_ERROR_CHECK(app_event_subscribe(APP_EVENT_QUOTE_FETCH_ROUND, on_quote_fetch_round_event, NULL));

    /* Phase 1: 硬體初始化 */
    ESP_LOGI(TAG, "初始化硬體...");
    ESP_ERROR_CHECK(board_init());

    /* 開機套用 LCD 亮度（在 UI 初始化前） */
    {
        uint8_t brightness = 80;
        ret = storage_display_load(&brightness);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "讀取亮度設定失敗，使用預設 80%%: %s", esp_err_to_name(ret));
            brightness = 80;
        }
        ret = board_set_lcd_brightness(brightness);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "套用亮度失敗: %s", esp_err_to_name(ret));
        }
    }

    /* Phase 2: LVGL UI 初始化 */
    ESP_LOGI(TAG, "初始化 LVGL UI...");
    ESP_ERROR_CHECK(ui_manager_init(g_ui_mutex));

    /* Phase 3: 儲存 & WiFi */
    ESP_LOGI(TAG, "初始化 Storage...");
    ESP_ERROR_CHECK(storage_init());
    ESP_ERROR_CHECK(storage_wifi_migrate_legacy());
    stock_list_t stocks = {0};
    if (storage_stocks_load(&stocks) == ESP_OK) {
        ui_manager_set_dashboard_symbols((const char (*)[8])stocks.symbols, stocks.count);
    }

    ESP_LOGI(TAG, "初始化 WiFi...");
    ESP_ERROR_CHECK(device_server_init());
    device_server_set_callback(on_wifi_state);
    device_server_set_stock_list_changed_callback(on_stock_list_changed);

    /* 嘗試自動連線；若無設定則導向 Portal 頁面，由使用者手動啟動配網入口 */
    ret = device_server_connect_saved();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "未連上既有 WiFi，請至 Portal 頁面手動啟動");
        ui_manager_switch_screen(SCREEN_PORTAL);
    }

    /* Phase 4: TWSE Client */
    ESP_LOGI(TAG, "初始化 TWSE Client...");
    ESP_ERROR_CHECK(twse_client_init(g_quote_queue));

    /* Phase 5: 排程器 */
    ESP_LOGI(TAG, "初始化 Scheduler...");
    ESP_ERROR_CHECK(scheduler_init(g_quote_queue));

    ESP_LOGI(TAG, "=== 系統啟動完成 ===");
    ui_manager_log_sys(LOG_LEVEL_INFO, "System ready");

    /* 主迴圈：消費 queue 資料 → 驅動 UI 更新 */
    stock_quote_t      quote;

    while (1) {
        static uint32_t s_last_diag_ms = 0;

        /* 只處理 Power 鍵短按：切換螢幕開/關 */
        board_poll_power_key();

        /* 每個 tick 最多處理一筆報價，避免一次鎖 mutex 多次與 LVGL 競爭 */
        if (xQueueReceive(g_quote_queue, &quote, 0) == pdTRUE) {
            ui_manager_update_quote(&quote);
        }

        /* 每 100ms 輪詢一次，同時監控堆疊健康度 */
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        if (free_heap < 8192) {
            ESP_LOGW(TAG, "heap 不足警告: %u bytes", free_heap);
        }

        ui_manager_heartbeat_feed_main();

        uint32_t now_ms = (uint32_t)esp_log_timestamp();
        if (now_ms - s_last_diag_ms >= 30000U) {
            UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "diag stack_hwm main=%u words free_heap=%u",
                     (unsigned)wm,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
            s_last_diag_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
