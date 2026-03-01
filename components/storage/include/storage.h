#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* ====== WiFi 設定 ====== */
esp_err_t storage_wifi_save(const char *ssid, const char *password);
esp_err_t storage_wifi_load(char *ssid, size_t ssid_size,
                             char *password, size_t pw_size);
bool      storage_wifi_has_saved(void);

/* ====== AI 設定 ====== */
esp_err_t storage_ai_save_provider(uint8_t provider_type);
esp_err_t storage_ai_load_provider(uint8_t *provider_type);
esp_err_t storage_ai_save_key(const char *api_key);
esp_err_t storage_ai_load_key(char *api_key, size_t size);
esp_err_t storage_ai_save_provider_key(uint8_t provider_type, const char *api_key);
esp_err_t storage_ai_load_provider_key(uint8_t provider_type, char *api_key, size_t size);
esp_err_t storage_ai_save_prompt_template(const char *prompt_template);
esp_err_t storage_ai_load_prompt_template(char *prompt_template, size_t size);

/* ====== 股票清單 ====== */
typedef struct {
    char symbols[10][8];  /* 最多 10 支，每個代號 ≤7 字元 */
    uint8_t count;
} stock_list_t;

esp_err_t storage_stocks_save(const stock_list_t *list);
esp_err_t storage_stocks_load(stock_list_t *list);

/* ====== 排程設定 ====== */
typedef struct {
    uint16_t quote_interval_s;   /* 報價更新間隔（秒）*/
    uint16_t ai_interval_min;    /* AI 分析間隔（分鐘，使用者可在機器上調整）*/
    bool     market_only;        /* 僅市場時段更新 */
} schedule_config_t;

esp_err_t storage_schedule_save(const schedule_config_t *cfg);
esp_err_t storage_schedule_load(schedule_config_t *cfg);

/* ====== 通用 ====== */
esp_err_t storage_init(void);
esp_err_t storage_erase_all(void);
