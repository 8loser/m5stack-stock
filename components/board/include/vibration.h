#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t vibration_init(void);
void      vibration_haptic(void);                  /* 短震：觸控回饋 */
void      vibration_alert(void);                   /* 長震：價格警報 */
void      vibration_pulse(uint32_t duration_ms);   /* 自訂震動時間 */
