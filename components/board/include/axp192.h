#pragma once
#include "esp_err.h"
#include "driver/i2c.h"

/* AXP192 暫存器 */
#define AXP192_REG_POWER_STATUS     0x00
#define AXP192_REG_CHARGE_CTRL1     0x33
#define AXP192_REG_DCDC1_VOLT       0x26  /* ESP32 core: 3.35V */
#define AXP192_REG_DCDC3_VOLT       0x27  /* Core2 背光電源 */
#define AXP192_REG_LDO2_LDO3_VOLT  0x28  /* LDO2=Core2 LCD電源, LDO3=Vib */
#define AXP192_REG_LDO_DCDC_EN     0x12
#define AXP192_REG_GPIO0_FUNC       0x90  /* 麥克風電源 */
#define AXP192_REG_GPIO1_FUNC       0x92  /* LED */
#define AXP192_REG_GPIO2_FUNC       0x93  /* 震動馬達 */
#define AXP192_REG_GPIO2_STATE      0x94
#define AXP192_REG_IRQ_EN3          0x42
#define AXP192_REG_IRQ_STS3         0x46
#define AXP192_REG_BATT_VOLT_H      0x78
#define AXP192_REG_BATT_VOLT_L      0x79

esp_err_t axp192_init(i2c_port_t port, uint8_t addr);
esp_err_t axp192_set_lcd_power(bool enable);
esp_err_t axp192_set_lcd_backlight(uint8_t brightness); /* 0-255 */
esp_err_t axp192_set_lcd_backlight_power(bool enable);
esp_err_t axp192_set_vibration(bool enable);
esp_err_t axp192_set_speaker_enable(bool enable);
esp_err_t axp192_set_bus_power(bool enable);
float     axp192_get_battery_voltage(void);
uint8_t   axp192_get_battery_percent(void);
bool      axp192_is_charging(void);
esp_err_t axp192_enable_pek_short_press_irq(void);
bool      axp192_consume_pek_short_press_event(void);

esp_err_t axp192_write_reg(uint8_t reg, uint8_t val);
esp_err_t axp192_read_reg(uint8_t reg, uint8_t *val);
