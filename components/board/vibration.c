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
    axp192_set_vibration(false);
    ESP_LOGI(TAG, "震動馬達初始化完成（AXP192 GPIO2）");
    return ESP_OK;
}

void vibration_pulse(uint32_t duration_ms)
{
    (void)duration_ms;
    /* Emergency safety mode: disable vibration completely until display issue is fixed. */
    axp192_set_vibration(false);
}

void vibration_haptic(void)
{
    vibration_pulse(VIBRATION_HAPTIC_MS);
}

void vibration_alert(void)
{
    axp192_set_vibration(false);
}
