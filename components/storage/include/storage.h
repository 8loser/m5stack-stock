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
esp_err_t storage_wifi_promote_ap(uint8_t idx);
esp_err_t storage_wifi_migrate_legacy(void);

/* ====== AI 設定 ====== */
esp_err_t storage_ai_save_provider(uint8_t provider_type);
esp_err_t storage_ai_load_provider(uint8_t *provider_type);
esp_err_t storage_ai_save_key(const char *api_key);
esp_err_t storage_ai_load_key(char *api_key, size_t size);
esp_err_t storage_ai_save_provider_key(uint8_t provider_type, const char *api_key);
esp_err_t storage_ai_load_provider_key(uint8_t provider_type, char *api_key, size_t size);
esp_err_t storage_ai_clear_provider_key(uint8_t provider_type);
esp_err_t storage_ai_clear_legacy_key(void);
esp_err_t storage_ai_save_global_prompt(const char *prompt);
esp_err_t storage_ai_load_global_prompt(char *prompt, size_t size);

/* ====== Telegram 設定 ====== */
esp_err_t storage_tg_save_enabled(bool enabled);
esp_err_t storage_tg_load_enabled(bool *enabled);
esp_err_t storage_tg_save_bot_token(const char *token);
esp_err_t storage_tg_load_bot_token(char *token, size_t size);
esp_err_t storage_tg_save_chat_id(const char *chat_id);
esp_err_t storage_tg_load_chat_id(char *chat_id, size_t size);

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

typedef struct {
    bool enabled;
    float up_threshold_pct;
    float down_threshold_pct;
    char alert_prompt[513];
} stock_alert_config_t;

esp_err_t storage_stock_alert_config_save(const char *symbol, const stock_alert_config_t *config);
esp_err_t storage_stock_alert_config_load(const char *symbol, stock_alert_config_t *config);
esp_err_t storage_stock_alert_config_remove(const char *symbol);

/* ====== AtTime 排程 ====== */
typedef struct {
    bool     enabled;
    uint8_t  hour;       /* 0-23 */
    uint8_t  minute;     /* 0-59 */
    uint8_t  weekdays;   /* bitmask: bit0=Sun, bit1=Mon, ..., bit6=Sat */
    char     prompt[AT_TIME_PROMPT_MAX_LEN + 1];
} at_time_entry_t;

esp_err_t storage_at_time_save_entry(uint8_t idx, const at_time_entry_t *entry);
esp_err_t storage_at_time_load_entry(uint8_t idx, at_time_entry_t *entry);
esp_err_t storage_at_time_remove_entry(uint8_t idx);
esp_err_t storage_at_time_save_count(uint8_t count);
uint8_t   storage_at_time_load_count(void);
esp_err_t storage_at_time_load_all(at_time_entry_t *entries, uint8_t *count);

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
