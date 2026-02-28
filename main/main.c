#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "board.h"
#include "storage.h"
#include "wifi_manager.h"
#include "twse_client.h"
#include "ai_provider.h"
#include "scheduler.h"
#include "ui_manager.h"

static const char *TAG = "main";

/* 全域共用資源 */
QueueHandle_t   g_quote_queue      = NULL;
QueueHandle_t   g_ai_result_queue  = NULL;
SemaphoreHandle_t g_ui_mutex       = NULL;

static void init_global_resources(void)
{
    g_quote_queue     = xQueueCreate(MAX_STOCK_COUNT, sizeof(stock_quote_t));
    g_ai_result_queue = xQueueCreate(1, sizeof(ai_analysis_result_t));
    g_ui_mutex        = xSemaphoreCreateMutex();

    if (!g_quote_queue || !g_ai_result_queue || !g_ui_mutex) {
        ESP_LOGE(TAG, "FreeRTOS 資源建立失敗");
        esp_restart();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== M5Stack Core2 台股監測 啟動 ===");

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

    /* Phase 1: 硬體初始化 */
    ESP_LOGI(TAG, "初始化硬體...");
    ESP_ERROR_CHECK(board_init());

    /* Phase 2: LVGL UI 初始化 */
    ESP_LOGI(TAG, "初始化 LVGL UI...");
    ESP_ERROR_CHECK(ui_manager_init(g_ui_mutex));

    /* Phase 3: 儲存 & WiFi */
    ESP_LOGI(TAG, "初始化 Storage...");
    ESP_ERROR_CHECK(storage_init());

    ESP_LOGI(TAG, "初始化 WiFi...");
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 嘗試自動連線 */
    wifi_manager_connect_saved();

    /* Phase 4: TWSE Client */
    ESP_LOGI(TAG, "初始化 TWSE Client...");
    ESP_ERROR_CHECK(twse_client_init(g_quote_queue));

    /* Phase 5: AI Provider */
    ESP_LOGI(TAG, "初始化 AI Provider...");
    ESP_ERROR_CHECK(ai_provider_init(g_ai_result_queue));

    /* Phase 6: 排程器 */
    ESP_LOGI(TAG, "初始化 Scheduler...");
    ESP_ERROR_CHECK(scheduler_init(g_quote_queue, g_ai_result_queue));

    ESP_LOGI(TAG, "=== 系統啟動完成 ===");

    /* 主迴圈：監控系統健康度 */
    while (1) {
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGD(TAG, "Heap: %u bytes, PSRAM: %u bytes", free_heap, free_psram);

        if (free_heap < 8192) {
            ESP_LOGW(TAG, "heap 不足警告: %u bytes", free_heap);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
