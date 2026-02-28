#include "axp192.h"
#include "app_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "axp192";
static i2c_port_t s_port;
static uint8_t    s_addr;

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

    /* DCDC3 = 2.8V (LCD VCC) */
    ret = axp192_write_reg(AXP192_REG_DCDC3_VOLT, 0x58);
    if (ret != ESP_OK) return ret;

    /* LDO2 = 3.3V (LCD 背光), LDO3 = 3.0V */
    ret = axp192_write_reg(AXP192_REG_LDO2_LDO3_VOLT, 0xCC);
    if (ret != ESP_OK) return ret;

    /* 啟用 DCDC1、DCDC3、LDO2、LDO3 */
    ret = axp192_write_reg(AXP192_REG_LDO_DCDC_EN, 0x4D);
    if (ret != ESP_OK) return ret;

    /* GPIO0 = 低電平輸出（麥克風電源關閉）*/
    ret = axp192_write_reg(AXP192_REG_GPIO0_FUNC, 0x07);
    if (ret != ESP_OK) return ret;

    /* GPIO2 = 低電平輸出（震動馬達關閉）*/
    ret = axp192_write_reg(AXP192_REG_GPIO2_FUNC, 0x07);
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
        val |= (1 << 2);  /* DCDC3 */
    } else {
        val &= ~(1 << 2);
    }
    return axp192_write_reg(AXP192_REG_LDO_DCDC_EN, val);
}

esp_err_t axp192_set_lcd_backlight(uint8_t brightness)
{
    /* LDO2 電壓對應亮度：1.8V(0x0) ~ 3.3V(0xF)
     * 映射 brightness(0-255) -> LDO2 nibble(0-15) */
    uint8_t nibble = (uint8_t)((uint32_t)brightness * 15 / 255);
    uint8_t val;
    esp_err_t ret = axp192_read_reg(AXP192_REG_LDO2_LDO3_VOLT, &val);
    if (ret != ESP_OK) return ret;

    val = (val & 0x0F) | (nibble << 4);
    return axp192_write_reg(AXP192_REG_LDO2_LDO3_VOLT, val);
}

esp_err_t axp192_set_vibration(bool enable)
{
    uint8_t val = enable ? 0x01 : 0x00;
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
