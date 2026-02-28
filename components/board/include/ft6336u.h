#pragma once
#include "esp_err.h"
#include "driver/i2c.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool     pressed;
} touch_point_t;

esp_err_t    ft6336u_init(i2c_port_t port, uint8_t addr);
esp_err_t    ft6336u_read(touch_point_t *point);
bool         ft6336u_is_touched(void);
