#include "sleep_manager.h"
#include "app_config.h"
#include "rtc_bm8563.h"
#include "axp192.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sleep_mgr";
static bool s_enabled = false;  /* 預設不啟用，使用者可在設定中開啟 */

esp_err_t sleep_manager_init(void)
{
    ESP_LOGI(TAG, "DeepSleep 管理器初始化（功能%s）",
             s_enabled ? "啟用" : "停用");
    return ESP_OK;
}

bool sleep_manager_should_sleep(void)
{
    if (!s_enabled) return false;

    rtc_time_t t;
    if (rtc_bm8563_get_time(&t) != ESP_OK) return false;

    uint16_t now   = t.hours * 60 + t.minutes;
    uint16_t close = MARKET_CLOSE_HOUR * 60 + MARKET_CLOSE_MIN;
    uint16_t open  = MARKET_OPEN_HOUR  * 60 + MARKET_OPEN_MIN;

    /* 休市時段：13:30 之後 或 09:00 之前 */
    return (now >= close || now < open);
}

void sleep_manager_enter(uint8_t wake_hour, uint8_t wake_min)
{
    ESP_LOGI(TAG, "進入 DeepSleep，喚醒時間: %02d:%02d", wake_hour, wake_min);

    /* 設定 BM8563 alarm 喚醒 */
    rtc_time_t alarm = {
        .hours   = wake_hour,
        .minutes = wake_min,
        .seconds = 0,
    };
    rtc_bm8563_set_alarm(&alarm);

    /* 設定 EXT1 喚醒（BM8563 INT 腳位：GPIO39）*/
    esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_INT_GPIO),
                                  ESP_EXT1_WAKEUP_ALL_LOW);

    /* 螢幕關閉 */
    axp192_set_lcd_backlight(0);
    axp192_set_lcd_power(false);

    vTaskDelay(pdMS_TO_TICKS(200));
    esp_deep_sleep_start();
    /* 不會執行到此 */
}

void sleep_manager_set_enabled(bool enable)
{
    s_enabled = enable;
    ESP_LOGI(TAG, "DeepSleep %s", enable ? "啟用" : "停用");
}

bool sleep_manager_is_enabled(void)
{
    return s_enabled;
}
