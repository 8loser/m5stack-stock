#pragma once
#include "app_config.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* ====== WiFi 設定 ====== */
esp_err_t storage_wifi_save(const char *ssid, const char *password);
esp_err_t storage_wifi_load(char *ssid, size_t ssid_size,
                             char *password, size_t pw_size);
bool      storage_wifi_has_saved(void);
uint8_t   storage_wifi_ap_count(void);
esp_err_t storage_wifi_save_ap(uint8_t idx, const char *ssid, const char *password);
esp_err_t storage_wifi_load_ap(uint8_t idx, char *ssid, size_t ssid_size,
                               char *password, size_t pw_size);
esp_err_t storage_wifi_remove_ap(uint8_t idx);
esp_err_t storage_wifi_add_ap(const char *ssid, const char *password);
esp_err_t storage_wifi_migrate_legacy(void);

/* ====== AI 設定 ====== */
esp_err_t storage_ai_save_provider(uint8_t provider_type);
esp_err_t storage_ai_load_provider(uint8_t *provider_type);
esp_err_t storage_ai_save_key(const char *api_key);
esp_err_t storage_ai_load_key(char *api_key, size_t size);
esp_err_t storage_ai_save_provider_key(uint8_t provider_type, const char *api_key);
esp_err_t storage_ai_load_provider_key(uint8_t provider_type, char *api_key, size_t size);

/* ====== 股票清單 ====== */
typedef struct {
    char symbols[MAX_STOCK_COUNT][8];  /* 最多 MAX_STOCK_COUNT 支，每個代號 ≤7 字元 */
    uint8_t count;
} stock_list_t;

esp_err_t storage_stocks_save(const stock_list_t *list);
esp_err_t storage_stocks_load(stock_list_t *list);

typedef struct {
    char name[64];
    char abbr[32];
    char industry[32];
} stock_meta_t;

esp_err_t storage_stock_meta_save(const char *symbol, const char *name,
                                  const char *abbr, const char *industry);
esp_err_t storage_stock_meta_load(const char *symbol, stock_meta_t *meta);
esp_err_t storage_stock_meta_remove(const char *symbol);

/* ====== 排程設定 ====== */
typedef struct {
    uint16_t quote_interval_s;   /* 報價更新間隔（秒）*/
    bool     market_only;        /* 僅市場時段更新 */
} schedule_config_t;

esp_err_t storage_schedule_save(const schedule_config_t *cfg);
esp_err_t storage_schedule_load(schedule_config_t *cfg);

/* ====== 顯示設定 ====== */
esp_err_t storage_display_save(uint8_t brightness);
esp_err_t storage_display_load(uint8_t *out);

/* ====== 通用 ====== */
esp_err_t storage_init(void);
esp_err_t storage_erase_all(void);
