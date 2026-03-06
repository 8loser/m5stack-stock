#pragma once

#include "esp_err.h"
#include "twse_models.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t telegram_bot_init(void);
esp_err_t telegram_bot_start(void);
esp_err_t telegram_bot_stop(void);
esp_err_t telegram_bot_pause_polling(void);
esp_err_t telegram_bot_resume_polling(void);
bool telegram_bot_is_running(void);
bool telegram_bot_is_polling_paused(void);
bool telegram_bot_is_http_in_flight(void);
esp_err_t telegram_bot_wait_http_idle(uint32_t timeout_ms);
esp_err_t telegram_bot_wait_stopped(uint32_t timeout_ms);
esp_err_t telegram_bot_cache_quote(const stock_quote_t *quote);
esp_err_t telegram_bot_send_text(const char *text);
