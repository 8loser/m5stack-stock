#pragma once

#include "esp_err.h"
#include "twse_models.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    esp_err_t err;
    int status_code;
    char detail[128];
} tg_cmd_send_test_result_t;

typedef struct {
    esp_err_t err;
    int status_code;
    char bot_name[64];
    char bot_username[64];
} tg_cmd_get_me_result_t;

typedef struct {
    esp_err_t err;
    int status_code;
    char *body; /* heap-allocated, caller frees */
} tg_cmd_get_updates_result_t;

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

esp_err_t telegram_bot_cmd_send_test(tg_cmd_send_test_result_t *out, uint32_t timeout_ms);
esp_err_t telegram_bot_cmd_get_me(const char *token_override, tg_cmd_get_me_result_t *out, uint32_t timeout_ms);
esp_err_t telegram_bot_cmd_get_updates(tg_cmd_get_updates_result_t *out, uint32_t timeout_ms);
