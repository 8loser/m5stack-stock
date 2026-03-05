#pragma once

#include "esp_err.h"
#include "twse_models.h"
#include <stdbool.h>

esp_err_t telegram_bot_init(void);
esp_err_t telegram_bot_start(void);
esp_err_t telegram_bot_stop(void);
esp_err_t telegram_bot_pause_polling(void);
esp_err_t telegram_bot_resume_polling(void);
bool telegram_bot_is_running(void);
bool telegram_bot_is_polling_paused(void);
esp_err_t telegram_bot_cache_quote(const stock_quote_t *quote);
