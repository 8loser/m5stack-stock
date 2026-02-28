#pragma once
#include "esp_err.h"
#include "axp192.h"
#include "ili9342c.h"
#include "ft6336u.h"
#include "rtc_bm8563.h"
#include "vibration.h"
#include "audio.h"

/**
 * @brief 初始化所有硬體（AXP192、LCD、Touch、RTC、震動、音效）
 */
esp_err_t board_init(void);

/**
 * @brief 取得硬體版本字串
 */
const char *board_get_version(void);
