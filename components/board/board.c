#include "board.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

static const char *TAG = "board";

static esp_lcd_panel_handle_t    s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io    = NULL;
static bool s_screen_on = true;
static int64_t s_power_key_arm_us = 0;

/* 共用 I2C 匯流排初始化 */
static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = TOUCH_SDA_GPIO,
        .scl_io_num       = TOUCH_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_PORT_NUM, &conf);
    if (ret != ESP_OK) return ret;
    return i2c_driver_install(I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t board_init(void)
{
    esp_err_t ret;

    /* 1. I2C 匯流排（AXP192, FT6336U, BM8563 共用）*/
    ret = init_i2c();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C 初始化失敗");
        return ret;
    }

    /* 2. AXP192 電源管理 */
    ret = axp192_init(I2C_PORT_NUM, AXP192_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AXP192 初始化失敗");
        return ret;
    }

    /* 3. LCD */
    ret = ili9342c_init(&s_panel, &s_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD 初始化失敗");
        return ret;
    }
    /* 保守起見，初始化後再次確保 LCD 供電/背光已開啟 */
    if (axp192_set_lcd_power(true) != ESP_OK) {
        ESP_LOGW(TAG, "LCD 供電開啟失敗");
    }
    if (axp192_set_lcd_backlight(255) != ESP_OK) {
        ESP_LOGW(TAG, "LCD 背光設定失敗");
    }
    s_screen_on = true;

    ret = axp192_enable_pek_short_press_irq();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Power 鍵短按中斷啟用失敗");
    }
    /* 避免開機/重置時殘留 PEK 事件造成進入主畫面後立刻熄屏 */
    s_power_key_arm_us = esp_timer_get_time() + 2000000;

    /* 4. 觸控 */
    ret = ft6336u_init(I2C_PORT_NUM, TOUCH_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "觸控初始化失敗");
        return ret;
    }

    /* 5. RTC */
    ret = rtc_bm8563_init(I2C_PORT_NUM, BM8563_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTC 初始化失敗");
        return ret;
    }

    /* 6. 震動馬達 */
    ret = vibration_init();
    if (ret != ESP_OK) return ret;

    /* 7. 音效 */
    ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "音效初始化失敗（可繼續）");
        /* 音效非關鍵，繼續執行 */
    }

    /* 開機震動回饋 */
    vibration_haptic();

    ESP_LOGI(TAG, "所有硬體初始化完成");
    return ESP_OK;
}

const char *board_get_version(void)
{
    return "M5Stack Core2 v1.0 (ESP32-D0WDQ6-V3)";
}

/* 提供給 LVGL 的 LCD panel handle */
esp_lcd_panel_handle_t board_get_panel(void)
{
    return s_panel;
}

esp_lcd_panel_io_handle_t board_get_panel_io(void)
{
    return s_io;
}

esp_err_t board_set_screen_on(bool on)
{
    esp_err_t ret;

    if (on) {
        ret = axp192_set_lcd_power(true);
        if (ret != ESP_OK) return ret;

        ret = axp192_set_lcd_backlight_power(true);
        if (ret != ESP_OK) return ret;

        ret = axp192_set_lcd_backlight(255);
        if (ret != ESP_OK) return ret;
    } else {
        ret = axp192_set_lcd_backlight(0);
        if (ret != ESP_OK) return ret;

        ret = axp192_set_lcd_backlight_power(false);
        if (ret != ESP_OK) return ret;
    }

    s_screen_on = on;
    ESP_LOGI(TAG, "螢幕%s", on ? "開啟" : "關閉");
    return ESP_OK;
}

bool board_is_screen_on(void)
{
    return s_screen_on;
}

esp_err_t board_set_lcd_brightness(uint8_t level)
{
    uint8_t clamped = level;
    uint8_t scaled;

    if (clamped > 100) clamped = 100;

    scaled = (uint8_t)(((uint32_t)clamped * 255U) / 100U);
    return axp192_set_lcd_backlight(scaled);
}

void board_poll_power_key(void)
{
    if (!axp192_consume_pek_short_press_event()) return;

    if (esp_timer_get_time() < s_power_key_arm_us) {
        ESP_LOGI(TAG, "忽略開機初期 Power 事件");
        return;
    }

    if (board_set_screen_on(!s_screen_on) != ESP_OK) {
        ESP_LOGW(TAG, "切換螢幕狀態失敗");
    }
}
