#pragma once

#include "esp_err.h"
#include "twse_models.h"
#include <stdbool.h>

esp_err_t telegram_bot_init(void);
esp_err_t telegram_bot_start(void);
esp_err_t telegram_bot_stop(void);
bool telegram_bot_is_running(void);
esp_err_t telegram_bot_cache_quote(const stock_quote_t *quote);
