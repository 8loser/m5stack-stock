#include "storage.h"
#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "storage";

static const char *provider_key_name(uint8_t provider_type)
{
    switch (provider_type) {
        case 0: return "key_gem";
        case 1: return "key_cla";
        case 2: return "key_oai";
        default: return NULL;
    }
}

esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "NVS Storage 就緒");
    return ESP_OK;
}

/* -------- WiFi -------- */

esp_err_t storage_wifi_save(const char *ssid, const char *password)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "password", password);
    ret = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi 設定已儲存 SSID=%s", ssid);
    return ret;
}

esp_err_t storage_wifi_load(char *ssid, size_t ssid_size,
                             char *password, size_t pw_size)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (ret != ESP_OK) return ret;

    nvs_get_str(h, "ssid", ssid, &ssid_size);
    nvs_get_str(h, "password", password, &pw_size);
    nvs_close(h);
    return ESP_OK;
}

bool storage_wifi_has_saved(void)
{
    char ssid[64] = {0};
    size_t ssid_size = sizeof(ssid);
    nvs_handle_t h;
    if (nvs_open(NVS_NS_WIFI, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t ret = nvs_get_str(h, "ssid", ssid, &ssid_size);
    nvs_close(h);
    return (ret == ESP_OK && strlen(ssid) > 0);
}

/* -------- AI -------- */

esp_err_t storage_ai_save_provider(uint8_t provider_type)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    nvs_set_u8(h, "provider", provider_type);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_ai_load_provider(uint8_t *provider_type)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        *provider_type = 0;  /* 預設 Gemini */
        return ESP_OK;
    }
    ret = nvs_get_u8(h, "provider", provider_type);
    if (ret == ESP_ERR_NVS_NOT_FOUND) *provider_type = 0;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_ai_save_key(const char *api_key)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    nvs_set_str(h, "api_key", api_key);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_ai_load_key(char *api_key, size_t size)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        api_key[0] = '\0';
        return ESP_OK;
    }
    ret = nvs_get_str(h, "api_key", api_key, &size);
    if (ret != ESP_OK) api_key[0] = '\0';
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_ai_save_provider_key(uint8_t provider_type, const char *api_key)
{
    const char *key_name = provider_key_name(provider_type);
    if (!key_name || !api_key) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    nvs_set_str(h, key_name, api_key);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_ai_load_provider_key(uint8_t provider_type, char *api_key, size_t size)
{
    const char *key_name = provider_key_name(provider_type);
    if (!key_name || !api_key || size == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        api_key[0] = '\0';
        return ESP_OK;
    }
    ret = nvs_get_str(h, key_name, api_key, &size);
    if (ret != ESP_OK) api_key[0] = '\0';
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_ai_save_prompt_template(const char *prompt_template)
{
    if (!prompt_template) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    nvs_set_str(h, "prompt_tpl", prompt_template);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_ai_load_prompt_template(char *prompt_template, size_t size)
{
    if (!prompt_template || size == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        prompt_template[0] = '\0';
        return ESP_OK;
    }
    ret = nvs_get_str(h, "prompt_tpl", prompt_template, &size);
    if (ret != ESP_OK) prompt_template[0] = '\0';
    nvs_close(h);
    return ESP_OK;
}

/* -------- 股票清單 -------- */

esp_err_t storage_stocks_save(const stock_list_t *list)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_STOCKS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_blob(h, "symbols", list->symbols, sizeof(list->symbols));
    nvs_set_u8(h, "count", list->count);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_stocks_load(stock_list_t *list)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_STOCKS, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        /* 預設股票清單 */
        const char *defaults[] = DEFAULT_STOCKS;
        list->count = DEFAULT_STOCK_COUNT;
        for (int i = 0; i < list->count; i++) {
            strncpy(list->symbols[i], defaults[i], 7);
            list->symbols[i][7] = '\0';
        }
        return ESP_OK;
    }

    size_t blob_size = sizeof(list->symbols);
    nvs_get_blob(h, "symbols", list->symbols, &blob_size);
    nvs_get_u8(h, "count", &list->count);
    nvs_close(h);
    return ESP_OK;
}

/* -------- 排程設定 -------- */

esp_err_t storage_schedule_save(const schedule_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_SCHEDULE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_u16(h, "quote_ivl", cfg->quote_interval_s);
    nvs_set_u16(h, "ai_ivl",    cfg->ai_interval_min);
    nvs_set_u8(h,  "mkt_only",  cfg->market_only ? 1 : 0);
    ret = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "排程設定已儲存：報價=%ds AI=%dmin",
             cfg->quote_interval_s, cfg->ai_interval_min);
    return ret;
}

esp_err_t storage_schedule_load(schedule_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_SCHEDULE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        /* 預設值 */
        cfg->quote_interval_s = DEFAULT_QUOTE_INTERVAL_S;
        cfg->ai_interval_min  = DEFAULT_AI_INTERVAL_MIN;
        cfg->market_only      = DEFAULT_MARKET_ONLY;
        return ESP_OK;
    }

    uint8_t mkt = 1;
    nvs_get_u16(h, "quote_ivl", &cfg->quote_interval_s);
    nvs_get_u16(h, "ai_ivl",    &cfg->ai_interval_min);
    nvs_get_u8(h,  "mkt_only",  &mkt);
    cfg->market_only = (mkt != 0);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_erase_all(void)
{
    esp_err_t ret = nvs_flash_erase();
    if (ret == ESP_OK) ret = nvs_flash_init();
    ESP_LOGW(TAG, "NVS 全部清除完成");
    return ret;
}
