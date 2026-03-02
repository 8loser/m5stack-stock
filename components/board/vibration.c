#include "vibration.h"
#include "axp192.h"
#include "app_config.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "vibration";
static esp_timer_handle_t s_vibration_off_timer = NULL;

static void vibration_off_timer_cb(void *arg)
{
    (void)arg;
    axp192_set_vibration(false);
}

esp_err_t vibration_init(void)
{
    if (s_vibration_off_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = vibration_off_timer_cb,
            .arg = NULL,
            .name = "vib_off",
            .dispatch_method = ESP_TIMER_TASK,
            .skip_unhandled_events = true,
        };
        esp_err_t timer_ret = esp_timer_create(&timer_args, &s_vibration_off_timer);
        if (timer_ret != ESP_OK) {
            ESP_LOGE(TAG, "建立 vibration timer 失敗: %s", esp_err_to_name(timer_ret));
            return timer_ret;
        }
    }

    /* AXP192 GPIO2 已在 axp192_init 設定為低電平輸出 */
    axp192_set_vibration(false);
    ESP_LOGI(TAG, "震動馬達初始化完成（AXP192 GPIO2）");
    return ESP_OK;
}

void vibration_pulse(uint32_t duration_ms)
{
    if (duration_ms == 0) {
        return;
    }

    if (s_vibration_off_timer == NULL) {
        return;
    }

    esp_timer_stop(s_vibration_off_timer);
    axp192_set_vibration(true);
    esp_timer_start_once(s_vibration_off_timer, (uint64_t)duration_ms * 1000ULL);
}

void vibration_haptic(void)
{
    vibration_pulse(VIBRATION_HAPTIC_MS);
}

void vibration_alert(void)
{
    vibration_pulse(VIBRATION_ALERT_MS);
}
