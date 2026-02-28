#include "vibration.h"
#include "axp192.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "vibration";

esp_err_t vibration_init(void)
{
    /* AXP192 GPIO2 已在 axp192_init 設定為低電平輸出 */
    ESP_LOGI(TAG, "震動馬達初始化完成（AXP192 GPIO2）");
    return ESP_OK;
}

void vibration_pulse(uint32_t duration_ms)
{
    axp192_set_vibration(true);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    axp192_set_vibration(false);
}

void vibration_haptic(void)
{
    vibration_pulse(VIBRATION_HAPTIC_MS);
}

void vibration_alert(void)
{
    /* 三段震動：ON-OFF-ON-OFF-ON */
    for (int i = 0; i < 3; i++) {
        axp192_set_vibration(true);
        vTaskDelay(pdMS_TO_TICKS(VIBRATION_ALERT_MS));
        axp192_set_vibration(false);
        if (i < 2) vTaskDelay(pdMS_TO_TICKS(100));
    }
}
