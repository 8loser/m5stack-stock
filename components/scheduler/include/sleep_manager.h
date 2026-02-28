#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化 DeepSleep 管理器
 */
esp_err_t sleep_manager_init(void);

/**
 * @brief 判斷是否應進入 DeepSleep（休市時段）
 *        台股：13:30 ~ 09:00（次日）為休市
 */
bool sleep_manager_should_sleep(void);

/**
 * @brief 進入 DeepSleep，由 RTC alarm 在開盤前 5 分鐘喚醒
 * @param wake_hour 喚醒小時
 * @param wake_min  喚醒分鐘
 */
void sleep_manager_enter(uint8_t wake_hour, uint8_t wake_min);

/**
 * @brief 設定 DeepSleep 功能開關
 */
void sleep_manager_set_enabled(bool enable);
bool sleep_manager_is_enabled(void);
