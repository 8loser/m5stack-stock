#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "axp192.h"  /* 供 sleep_manager 存取 axp192_set_lcd_power */
#include <time.h>

#define BM_REG_ALARM_MIN    0x09

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

esp_err_t rtc_bm8563_init(i2c_master_bus_handle_t bus, uint8_t addr);
esp_err_t rtc_bm8563_get_time(rtc_time_t *t);
esp_err_t rtc_bm8563_set_time(const rtc_time_t *t);
esp_err_t rtc_bm8563_set_alarm(const rtc_time_t *alarm);
esp_err_t rtc_bm8563_clear_alarm(void);
bool      rtc_bm8563_is_market_open(void);  /* 09:00 - 13:30 */
time_t    rtc_bm8563_to_unix(const rtc_time_t *t);
