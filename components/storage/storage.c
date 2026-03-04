#include "storage.h"
#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "storage";
static const char *NVS_NS_DISPLAY = "display";
static const uint8_t DEFAULT_DISPLAY_BRIGHTNESS = 80;

static const char *provider_key_name(uint8_t provider_type)
{
    switch (provider_type) {
        case 0: return "key_gem";
        case 1: return "key_cla";
        case 2: return "key_oai";
        default: return NULL;
    }
}

static bool is_stock_symbol_valid(const char *symbol)
{
    return symbol && symbol[0] != '\0' && strlen(symbol) <= 7;
}

static void make_stock_meta_key(char *buf, size_t buf_size, const char *symbol)
{
    snprintf(buf, buf_size, "m_%s", symbol);
}

static esp_err_t migrate_ai_prompt_template_cleanup(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_AI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    ret = nvs_erase_key(h, "prompt_tpl");
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        nvs_close(h);
        return ret;
    }

    ret = nvs_commit(h);
    nvs_close(h);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AI prompt_tpl 已清除（遷移）");
    }
    return ret;
}

esp_err_t storage_init(void)
{
    esp_err_t ret = migrate_ai_prompt_template_cleanup();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AI prompt_tpl 遷移清除失敗: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "NVS Storage 就緒");
    return ESP_OK;
}

/* -------- WiFi -------- */

static void make_ap_ssid_key(char *buf, size_t buf_size, uint8_t idx)
{
    snprintf(buf, buf_size, "ap_%d_ssid", idx);
}

static void make_ap_pass_key(char *buf, size_t buf_size, uint8_t idx)
{
    snprintf(buf, buf_size, "ap_%d_pass", idx);
}

static esp_err_t load_ap_from_handle(nvs_handle_t h, uint8_t idx,
                                     char *ssid, size_t ssid_size,
                                     char *password, size_t pw_size)
{
    char ssid_key[16] = {0};
    char pass_key[16] = {0};
    make_ap_ssid_key(ssid_key, sizeof(ssid_key), idx);
    make_ap_pass_key(pass_key, sizeof(pass_key), idx);

    esp_err_t ret = nvs_get_str(h, ssid_key, ssid, &ssid_size);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_str(h, pass_key, password, &pw_size);
    if (ret != ESP_OK) return ret;
    return ESP_OK;
}

static esp_err_t save_ap_to_handle(nvs_handle_t h, uint8_t idx, const char *ssid, const char *password)
{
    char ssid_key[16] = {0};
    char pass_key[16] = {0};
    make_ap_ssid_key(ssid_key, sizeof(ssid_key), idx);
    make_ap_pass_key(pass_key, sizeof(pass_key), idx);

    esp_err_t ret = nvs_set_str(h, ssid_key, ssid);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_str(h, pass_key, password);
    if (ret != ESP_OK) return ret;
    return ESP_OK;
}

uint8_t storage_wifi_ap_count(void)
{
    uint8_t count = 0;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (ret != ESP_OK) return 0;

    ret = nvs_get_u8(h, "ap_count", &count);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (ret != ESP_OK) return 0;
    if (count > WIFI_MAX_AP_COUNT) return WIFI_MAX_AP_COUNT;
    return count;
}

esp_err_t storage_wifi_save_ap(uint8_t idx, const char *ssid, const char *password)
{
    if (!ssid || !password || idx >= WIFI_MAX_AP_COUNT) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    ret = save_ap_to_handle(h, idx, ssid, password);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_wifi_load_ap(uint8_t idx, char *ssid, size_t ssid_size,
                               char *password, size_t pw_size)
{
    if (!ssid || !password || idx >= WIFI_MAX_AP_COUNT) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (ret != ESP_OK) return ret;

    ret = load_ap_from_handle(h, idx, ssid, ssid_size, password, pw_size);
    nvs_close(h);
    return ret;
}

esp_err_t storage_wifi_remove_ap(uint8_t idx)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    uint8_t count = 0;
    ret = nvs_get_u8(h, "ap_count", &count);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK || idx >= count) {
        nvs_close(h);
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = idx; i + 1 < count; i++) {
        char next_ssid[WIFI_SSID_MAX_LEN] = {0};
        char next_pass[WIFI_PASS_MAX_LEN] = {0};
        ret = load_ap_from_handle(h, i + 1, next_ssid, sizeof(next_ssid), next_pass, sizeof(next_pass));
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
        ret = save_ap_to_handle(h, i, next_ssid, next_pass);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
    }

    char last_ssid_key[16] = {0};
    char last_pass_key[16] = {0};
    make_ap_ssid_key(last_ssid_key, sizeof(last_ssid_key), (uint8_t)(count - 1));
    make_ap_pass_key(last_pass_key, sizeof(last_pass_key), (uint8_t)(count - 1));
    ret = nvs_erase_key(h, last_ssid_key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return ret;
    }
    ret = nvs_erase_key(h, last_pass_key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return ret;
    }

    ret = nvs_set_u8(h, "ap_count", (uint8_t)(count - 1));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_wifi_add_ap(const char *ssid, const char *password)
{
    if (!ssid || !password || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    uint8_t count = 0;
    ret = nvs_get_u8(h, "ap_count", &count);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        count = 0;
        ret = ESP_OK;
    }
    if (ret != ESP_OK) {
        nvs_close(h);
        return ret;
    }
    if (count > WIFI_MAX_AP_COUNT) count = WIFI_MAX_AP_COUNT;

    for (uint8_t i = 0; i < count; i++) {
        char saved_ssid[WIFI_SSID_MAX_LEN] = {0};
        size_t ssid_sz = sizeof(saved_ssid);
        char ssid_key[16] = {0};
        make_ap_ssid_key(ssid_key, sizeof(ssid_key), i);
        ret = nvs_get_str(h, ssid_key, saved_ssid, &ssid_sz);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
        if (strcmp(saved_ssid, ssid) == 0) {
            char pass_key[16] = {0};
            make_ap_pass_key(pass_key, sizeof(pass_key), i);
            ret = nvs_set_str(h, pass_key, password);
            if (ret == ESP_OK) ret = nvs_commit(h);
            nvs_close(h);
            return ret;
        }
    }

    if (count >= WIFI_MAX_AP_COUNT) {
        for (uint8_t i = 0; i + 1 < count; i++) {
            char next_ssid[WIFI_SSID_MAX_LEN] = {0};
            char next_pass[WIFI_PASS_MAX_LEN] = {0};
            ret = load_ap_from_handle(h, i + 1, next_ssid, sizeof(next_ssid), next_pass, sizeof(next_pass));
            if (ret != ESP_OK) {
                nvs_close(h);
                return ret;
            }
            ret = save_ap_to_handle(h, i, next_ssid, next_pass);
            if (ret != ESP_OK) {
                nvs_close(h);
                return ret;
            }
        }
        char last_ssid_key[16] = {0};
        char last_pass_key[16] = {0};
        make_ap_ssid_key(last_ssid_key, sizeof(last_ssid_key), (uint8_t)(count - 1));
        make_ap_pass_key(last_pass_key, sizeof(last_pass_key), (uint8_t)(count - 1));
        ret = nvs_erase_key(h, last_ssid_key);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(h);
            return ret;
        }
        ret = nvs_erase_key(h, last_pass_key);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(h);
            return ret;
        }
        count--;
    }

    ret = save_ap_to_handle(h, count, ssid, password);
    if (ret != ESP_OK) {
        nvs_close(h);
        return ret;
    }
    ret = nvs_set_u8(h, "ap_count", (uint8_t)(count + 1));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_wifi_migrate_legacy(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    uint8_t count = 0;
    ret = nvs_get_u8(h, "ap_count", &count);
    if (ret == ESP_OK) {
        nvs_close(h);
        return ESP_OK;
    }
    if (ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return ret;
    }

    char legacy_ssid[WIFI_SSID_MAX_LEN] = {0};
    char legacy_password[WIFI_PASS_MAX_LEN] = {0};
    size_t ssid_sz = sizeof(legacy_ssid);
    ret = nvs_get_str(h, "ssid", legacy_ssid, &ssid_sz);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = nvs_set_u8(h, "ap_count", 0);
        if (ret == ESP_OK) ret = nvs_commit(h);
        if (ret == ESP_OK) ESP_LOGI(TAG, "WiFi AP 清單初始化 ap_count=0");
        nvs_close(h);
        return ret;
    }
    if (ret != ESP_OK) {
        nvs_close(h);
        return ret;
    }

    size_t pass_sz = sizeof(legacy_password);
    ret = nvs_get_str(h, "password", legacy_password, &pass_sz);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        legacy_password[0] = '\0';
        ret = ESP_OK;
    }
    if (ret != ESP_OK) {
        nvs_close(h);
        return ret;
    }

    ret = save_ap_to_handle(h, 0, legacy_ssid, legacy_password);
    if (ret == ESP_OK) ret = nvs_set_u8(h, "ap_count", 1);
    if (ret == ESP_OK) ret = nvs_erase_key(h, "ssid");
    if (ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    if (ret == ESP_OK) ret = nvs_erase_key(h, "password");
    if (ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    if (ret == ESP_OK) ret = nvs_commit(h);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi 設定已從舊格式遷移");
    }
    nvs_close(h);
    return ret;
}

esp_err_t storage_wifi_save(const char *ssid, const char *password)
{
    esp_err_t ret = storage_wifi_add_ap(ssid, password);
    if (ret == ESP_OK) ESP_LOGI(TAG, "WiFi 設定已儲存 SSID=%s", ssid);
    return ret;
}

esp_err_t storage_wifi_load(char *ssid, size_t ssid_size,
                             char *password, size_t pw_size)
{
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    if (storage_wifi_ap_count() == 0) return ESP_ERR_NOT_FOUND;
    return storage_wifi_load_ap(0, ssid, ssid_size, password, pw_size);
}

bool storage_wifi_has_saved(void)
{
    return storage_wifi_ap_count() > 0;
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
        size_t defaults_count = sizeof(defaults) / sizeof(defaults[0]);

        list->count = (uint8_t)defaults_count;
        if (list->count > MAX_STOCK_COUNT) {
            list->count = MAX_STOCK_COUNT;
        }

        for (uint8_t i = 0; i < list->count; i++) {
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

esp_err_t storage_stock_meta_save(const char *symbol, const char *name,
                                  const char *abbr, const char *industry)
{
    if (!is_stock_symbol_valid(symbol)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_STOCKS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    stock_meta_t meta = {0};
    strlcpy(meta.name, name ? name : "", sizeof(meta.name));
    strlcpy(meta.abbr, abbr ? abbr : "", sizeof(meta.abbr));
    strlcpy(meta.industry, industry ? industry : "", sizeof(meta.industry));

    char key[16] = {0};
    make_stock_meta_key(key, sizeof(key), symbol);
    ret = nvs_set_blob(h, key, &meta, sizeof(meta));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t storage_stock_meta_load(const char *symbol, stock_meta_t *meta)
{
    if (!is_stock_symbol_valid(symbol) || !meta) return ESP_ERR_INVALID_ARG;
    memset(meta, 0, sizeof(*meta));

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_STOCKS, NVS_READONLY, &h);
    if (ret != ESP_OK) return ret;

    char key[16] = {0};
    make_stock_meta_key(key, sizeof(key), symbol);
    size_t sz = sizeof(*meta);
    ret = nvs_get_blob(h, key, meta, &sz);
    nvs_close(h);
    if (ret != ESP_OK) return ret;
    if (sz != sizeof(*meta)) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

esp_err_t storage_stock_meta_remove(const char *symbol)
{
    if (!is_stock_symbol_valid(symbol)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_STOCKS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    char key[16] = {0};
    make_stock_meta_key(key, sizeof(key), symbol);
    ret = nvs_erase_key(h, key);
    if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

/* -------- 排程設定 -------- */

esp_err_t storage_schedule_save(const schedule_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_SCHEDULE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_u16(h, "quote_ivl", cfg->quote_interval_s);
    nvs_set_u8(h,  "mkt_only",  cfg->market_only ? 1 : 0);
    ret = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "排程設定已儲存：報價=%ds market_only=%d",
             cfg->quote_interval_s, cfg->market_only ? 1 : 0);
    return ret;
}

esp_err_t storage_schedule_load(schedule_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_SCHEDULE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        /* 預設值 */
        cfg->quote_interval_s = DEFAULT_QUOTE_INTERVAL_S;
        cfg->market_only      = DEFAULT_MARKET_ONLY;
        return ESP_OK;
    }

    uint8_t mkt = 1;
    nvs_get_u16(h, "quote_ivl", &cfg->quote_interval_s);
    nvs_get_u8(h,  "mkt_only",  &mkt);
    cfg->market_only = (mkt != 0);
    nvs_close(h);
    return ESP_OK;
}

/* -------- 顯示設定 -------- */

esp_err_t storage_display_save(uint8_t brightness)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_DISPLAY, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    if (brightness > 100) brightness = 100;
    ret = nvs_set_u8(h, "brightness", brightness);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "顯示亮度已儲存：%u%%", (unsigned)brightness);
    }
    return ret;
}

esp_err_t storage_display_load(uint8_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_DISPLAY, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = DEFAULT_DISPLAY_BRIGHTNESS;
        return ESP_OK;
    }
    if (ret != ESP_OK) return ret;

    ret = nvs_get_u8(h, "brightness", out);
    nvs_close(h);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = DEFAULT_DISPLAY_BRIGHTNESS;
        return ESP_OK;
    }
    if (ret != ESP_OK) return ret;
    if (*out > 100) *out = 100;
    return ESP_OK;
}

esp_err_t storage_erase_all(void)
{
    esp_err_t ret = nvs_flash_erase();
    if (ret == ESP_OK) ret = nvs_flash_init();
    ESP_LOGW(TAG, "NVS 全部清除完成");
    return ret;
}
