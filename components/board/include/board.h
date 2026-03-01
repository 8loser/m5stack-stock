#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
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

/**
 * @brief 取得 LCD panel handle（供 LVGL flush callback 使用）
 */
esp_lcd_panel_handle_t    board_get_panel(void);
esp_lcd_panel_io_handle_t board_get_panel_io(void);

/**
 * @brief 設定螢幕是否開啟（開啟=LCD供電+背光，關閉=關閉背光）
 */
esp_err_t board_set_screen_on(bool on);

/**
 * @brief 取得目前螢幕狀態
 */
bool board_is_screen_on(void);

/**
 * @brief 輪詢 Power 鍵短按事件並切換螢幕狀態
 */
void board_poll_power_key(void);
