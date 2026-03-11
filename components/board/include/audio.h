#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t audio_init(void);
void      audio_beep(uint32_t freq_hz, uint32_t duration_ms);  /* 單音蜂鳴 */
void      audio_notify(void);      /* AI 分析完成提示音 */
void      audio_deinit(void);
