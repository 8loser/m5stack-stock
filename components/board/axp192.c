#include "axp192.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "axp192";
static i2c_port_t s_port;
static uint8_t    s_addr;

/* REG 0x12 power enable bits */
#define PWR_EN_DCDC1    (1U << 0)
#define PWR_EN_DCDC3    (1U << 1)
#define PWR_EN_LDO2     (1U << 2)
#define PWR_EN_LDO3     (1U << 3)
#define PWR_EN_DCDC2    (1U << 4)
#define PWR_EN_EXTEN    (1U << 6)

/* REG 0x94 GPIO state bits */
#define GPIO_STATE_GPIO2    (1U << 2)
/* PEK 短按事件 */
#define IRQ_PEK_SHORT_PRESS (1U << 1)

esp_err_t axp192_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_port, s_addr, buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t axp192_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(s_port, s_addr, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t axp192_init(i2c_port_t port, uint8_t addr)
{
    s_port = port;
    s_addr = addr;

    esp_err_t ret;

    /* DCDC1 = 3.35V (ESP32 VDD) */
    ret = axp192_write_reg(AXP192_REG_DCDC1_VOLT, 0x68);
    if (ret != ESP_OK) return ret;

    /* DCDC3 = 2.8V（Core2 背光電源） */
    ret = axp192_write_reg(AXP192_REG_DCDC3_VOLT, 0x58);
    if (ret != ESP_OK) return ret;

    /* LDO2 = 3.3V（Core2 LCD 邏輯電源）, LDO3 = 1.8V（震動馬達） */
    ret = axp192_write_reg(AXP192_REG_LDO2_LDO3_VOLT, 0xC0);
    if (ret != ESP_OK) return ret;

    /* 啟用 ESP32 與 LCD 必要電源；LDO3 預設關閉避免上電即震動 */
    ret = axp192_write_reg(AXP192_REG_LDO_DCDC_EN, PWR_EN_DCDC1 | PWR_EN_DCDC3 | PWR_EN_LDO2 | PWR_EN_EXTEN);
    if (ret != ESP_OK) return ret;

    /* GPIO0 = 低電平輸出（麥克風電源關閉）*/
    ret = axp192_write_reg(AXP192_REG_GPIO0_FUNC, 0x07);
    if (ret != ESP_OK) return ret;

    /* GPIO2 = 輸出低電平（震動馬達關閉）*/
    ret = axp192_write_reg(AXP192_REG_GPIO2_FUNC, 0x00);
    if (ret != ESP_OK) return ret;
    ret = axp192_set_vibration(false);
    if (ret != ESP_OK) return ret;

    /* 充電設定：4.2V, 780mA */
    ret = axp192_write_reg(AXP192_REG_CHARGE_CTRL1, 0xC0);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "AXP192 初始化完成");
    return ESP_OK;
}

esp_err_t axp192_set_lcd_power(bool enable)
{
    uint8_t val;
    esp_err_t ret = axp192_read_reg(AXP192_REG_LDO_DCDC_EN, &val);
    if (ret != ESP_OK) return ret;

    if (enable) {
        val |= PWR_EN_LDO2;
    } else {
        val &= ~PWR_EN_LDO2;
    }
    return axp192_write_reg(AXP192_REG_LDO_DCDC_EN, val);
}

esp_err_t axp192_set_lcd_backlight_power(bool enable)
{
    uint8_t val;
    esp_err_t ret = axp192_read_reg(AXP192_REG_LDO_DCDC_EN, &val);
    if (ret != ESP_OK) return ret;

    if (enable) {
        val |= PWR_EN_DCDC3;
    } else {
        val &= ~PWR_EN_DCDC3;
    }
    return axp192_write_reg(AXP192_REG_LDO_DCDC_EN, val);
}

esp_err_t axp192_set_lcd_backlight(uint8_t brightness)
{
    /* DCDC3 電壓對應背光亮度：
     * 近似映射 brightness(0-255) -> 2.5V~3.3V */
    uint16_t mv = 2500U + ((uint32_t)brightness * 800U) / 255U;
    uint8_t reg = (uint8_t)((mv - 700U) / 25U);
    return axp192_write_reg(AXP192_REG_DCDC3_VOLT, reg);
}

esp_err_t axp192_set_vibration(bool enable)
{
    uint8_t val;
    esp_err_t ret = axp192_read_reg(AXP192_REG_GPIO2_STATE, &val);
    if (ret != ESP_OK) return ret;

    if (enable) {
        val |= GPIO_STATE_GPIO2;
    } else {
        val &= ~GPIO_STATE_GPIO2;
    }
    return axp192_write_reg(AXP192_REG_GPIO2_STATE, val);
}

float axp192_get_battery_voltage(void)
{
    uint8_t h, l;
    if (axp192_read_reg(AXP192_REG_BATT_VOLT_H, &h) != ESP_OK) return 0.0f;
    if (axp192_read_reg(AXP192_REG_BATT_VOLT_L, &l) != ESP_OK) return 0.0f;

    uint16_t raw = ((uint16_t)(h & 0x7F) << 5) | (l & 0x1F);
    return (float)raw * 1.1f / 1000.0f;  /* mV -> V */
}

uint8_t axp192_get_battery_percent(void)
{
    float v = axp192_get_battery_voltage();
    if (v >= 4.2f) return 100;
    if (v <= 3.0f) return 0;
    return (uint8_t)((v - 3.0f) / (4.2f - 3.0f) * 100.0f);
}

bool axp192_is_charging(void)
{
    uint8_t val;
    axp192_read_reg(AXP192_REG_POWER_STATUS, &val);
    return (val & 0x04) != 0;
}

esp_err_t axp192_enable_pek_short_press_irq(void)
{
    uint8_t val = 0;
    esp_err_t ret = axp192_read_reg(AXP192_REG_IRQ_EN3, &val);
    if (ret != ESP_OK) return ret;

    val |= IRQ_PEK_SHORT_PRESS;
    ret = axp192_write_reg(AXP192_REG_IRQ_EN3, val);
    if (ret != ESP_OK) return ret;

    /* 清掉既有 pending 狀態，避免上電後立刻誤判一次按鍵 */
    return axp192_write_reg(AXP192_REG_IRQ_STS3, IRQ_PEK_SHORT_PRESS);
}

bool axp192_consume_pek_short_press_event(void)
{
    static int64_t s_last_event_us = 0;
    uint8_t sts = 0;
    if (axp192_read_reg(AXP192_REG_IRQ_STS3, &sts) != ESP_OK) {
        return false;
    }

    bool pressed = (sts & IRQ_PEK_SHORT_PRESS) != 0;
    if (!pressed) return false;

    /* 防抖：忽略 300ms 內重複事件；先判斷再清暫存器，避免提前吃掉事件 */
    int64_t now = esp_timer_get_time();
    if ((now - s_last_event_us) < 300000) {
        return false;
    }

    /* Write-1-to-clear */
    axp192_write_reg(AXP192_REG_IRQ_STS3, IRQ_PEK_SHORT_PRESS);
    s_last_event_us = now;
    return true;
}

esp_err_t axp192_set_bus_power(bool enable)
{
    /* M5Core2 外部匯流排 5V（EXTEN）由 AXP192 GPIO0 控制 */
    uint8_t val = enable ? 0x02 : 0x07;  /* 0x02=低電平驅動輸出, 0x07=浮動 */
    return axp192_write_reg(AXP192_REG_GPIO0_FUNC, val);
}
