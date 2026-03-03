#include "screen_info.h"
#include "app_config.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "storage.h"
#include "ui_compat.h"
#include "device_server.h"
#include <stdio.h>
#include <string.h>

#define COLOR_BG lv_color_hex(0x122113)
#define COLOR_SECTION lv_color_hex(0x4FC3F7)

static lv_obj_t *s_section_lbl_0 = NULL;
static lv_obj_t *s_section_lbl_1 = NULL;
static lv_obj_t *s_section_lbl_2 = NULL;
static lv_obj_t *s_section_lbl_3 = NULL;
static lv_obj_t *s_content_lbl_0 = NULL;
static lv_obj_t *s_content_lbl_1 = NULL;
static lv_obj_t *s_content_lbl_2 = NULL;
static lv_obj_t *s_content_lbl_3 = NULL;

static const lv_font_t *info_font(void)
{
    return &lv_font_noto_tc_14;
}

static const char *provider_to_text(uint8_t provider)
{
    switch (provider) {
    case 0:
        return "Gemini";
    case 1:
        return "Claude";
    case 2:
        return "OpenAI";
    default:
        return "未設定";
    }
}

static const char *wifi_state_to_text(wifi_state_t state)
{
    switch (state) {
    case WIFI_STATE_CONNECTED:
        return "已連線";
    case WIFI_STATE_CONNECTING:
        return "連線中";
    case WIFI_STATE_DISCONNECTED:
    case WIFI_STATE_FAILED:
    default:
        return "離線";
    }
}

static const char *chip_model_to_text(esp_chip_model_t model)
{
    switch (model) {
#ifdef CHIP_ESP32
    case CHIP_ESP32:
        return "ESP32";
#endif
#ifdef CHIP_ESP32S2
    case CHIP_ESP32S2:
        return "ESP32-S2";
#endif
#ifdef CHIP_ESP32S3
    case CHIP_ESP32S3:
        return "ESP32-S3";
#endif
#ifdef CHIP_ESP32C3
    case CHIP_ESP32C3:
        return "ESP32-C3";
#endif
#ifdef CHIP_ESP32C2
    case CHIP_ESP32C2:
        return "ESP32-C2";
#endif
#ifdef CHIP_ESP32C6
    case CHIP_ESP32C6:
        return "ESP32-C6";
#endif
#ifdef CHIP_ESP32H2
    case CHIP_ESP32H2:
        return "ESP32-H2";
#endif
#ifdef CHIP_ESP32P4
    case CHIP_ESP32P4:
        return "ESP32-P4";
#endif
    default:
        return "Unknown";
    }
}

void screen_info_refresh(void)
{
    if (s_content_lbl_0 == NULL || s_content_lbl_1 == NULL ||
        s_content_lbl_2 == NULL || s_content_lbl_3 == NULL) {
        return;
    }

    {
        esp_chip_info_t chip_info = {0};
        char device_text[128];
        esp_chip_info(&chip_info);
        snprintf(device_text, sizeof(device_text),
                 "記憶體: %lu bytes\n晶片: %s %d-core\nIDF: %s",
                 (unsigned long)esp_get_free_heap_size(),
                 chip_model_to_text(chip_info.model),
                 (int)chip_info.cores,
                 esp_get_idf_version());
        lv_label_set_text(s_content_lbl_0, device_text);
    }

    {
        char ssid[33] = {0};
        char password[65] = {0};
        char network_text[160];
        const char *display_ssid = "未儲存";
        const char *display_ip = "--";
        wifi_state_t state = device_server_get_state();
        const char *ip = device_server_get_ip();

        if (storage_wifi_load(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK &&
            ssid[0] != '\0') {
            display_ssid = ssid;
        }
        if (state == WIFI_STATE_CONNECTED && ip != NULL && ip[0] != '\0') {
            display_ip = ip;
        }

        snprintf(network_text, sizeof(network_text),
                 "SSID: %s\n狀態: %s\nIP: %s",
                 display_ssid,
                 wifi_state_to_text(state),
                 display_ip);
        lv_label_set_text(s_content_lbl_1, network_text);
    }

    {
        uint8_t provider = 0;
        char api_key[128] = {0};
        char ai_text[192];
        const char *provider_name;
        const char *api_key_state = "未設定";

        storage_ai_load_provider(&provider);
        storage_ai_load_provider_key(provider, api_key, sizeof(api_key));

        if (api_key[0] != '\0') {
            api_key_state = "已設定";
        }
        provider_name = provider_to_text(provider);

        snprintf(ai_text, sizeof(ai_text),
                 "供應商: %s\nAPI Key: %s",
                 provider_name, api_key_state);
        lv_label_set_text(s_content_lbl_2, ai_text);
    }

    {
        schedule_config_t schedule = {0};
        stock_list_t stocks = {0};
        char symbols_text[200] = {0};
        char stocks_text[320];
        size_t pos = 0;

        storage_schedule_load(&schedule);
        storage_stocks_load(&stocks);

        if (stocks.count == 0) {
            strlcpy(symbols_text, "未設定", sizeof(symbols_text));
        } else {
            for (uint8_t i = 0; i < stocks.count && pos + 1 < sizeof(symbols_text); i++) {
                int n = snprintf(symbols_text + pos, sizeof(symbols_text) - pos, "%s%s",
                                 (i == 0) ? "" : " ", stocks.symbols[i]);
                if (n < 0) {
                    break;
                }
                pos += (size_t)n;
                if (pos >= sizeof(symbols_text)) {
                    symbols_text[sizeof(symbols_text) - 1] = '\0';
                    break;
                }
            }
        }

        snprintf(stocks_text, sizeof(stocks_text),
                 "報價: %u 秒  僅盤中: %s\n代號: %s",
                 (unsigned)schedule.quote_interval_s,
                 schedule.market_only ? "是" : "否",
                 symbols_text);
        lv_label_set_text(s_content_lbl_3, stocks_text);
    }
}

lv_obj_t *screen_info_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *container = lv_obj_create(screen);
    lv_obj_set_pos(container, 0, UI_CONTENT_TOP_Y + 8);
    lv_obj_set_size(container, LCD_WIDTH, 190);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 4, 0);
    lv_obj_set_style_pad_row(container, 6, 0);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    s_section_lbl_0 = lv_label_create(container);
    lv_label_set_text(s_section_lbl_0, "裝置");
    lv_obj_set_width(s_section_lbl_0, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_section_lbl_0, COLOR_SECTION, 0);
    lv_obj_set_style_text_font(s_section_lbl_0, info_font(), 0);

    s_content_lbl_0 = lv_label_create(container);
    lv_obj_set_width(s_content_lbl_0, LCD_WIDTH - 16);
    lv_label_set_long_mode(s_content_lbl_0, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_content_lbl_0, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_content_lbl_0, info_font(), 0);

    s_section_lbl_1 = lv_label_create(container);
    lv_label_set_text(s_section_lbl_1, "網路");
    lv_obj_set_width(s_section_lbl_1, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_section_lbl_1, COLOR_SECTION, 0);
    lv_obj_set_style_text_font(s_section_lbl_1, info_font(), 0);

    s_content_lbl_1 = lv_label_create(container);
    lv_obj_set_width(s_content_lbl_1, LCD_WIDTH - 16);
    lv_label_set_long_mode(s_content_lbl_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_content_lbl_1, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_content_lbl_1, info_font(), 0);

    s_section_lbl_2 = lv_label_create(container);
    lv_label_set_text(s_section_lbl_2, "AI");
    lv_obj_set_width(s_section_lbl_2, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_section_lbl_2, COLOR_SECTION, 0);
    lv_obj_set_style_text_font(s_section_lbl_2, info_font(), 0);

    s_content_lbl_2 = lv_label_create(container);
    lv_obj_set_width(s_content_lbl_2, LCD_WIDTH - 16);
    lv_label_set_long_mode(s_content_lbl_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_content_lbl_2, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_content_lbl_2, info_font(), 0);

    s_section_lbl_3 = lv_label_create(container);
    lv_label_set_text(s_section_lbl_3, "股票");
    lv_obj_set_width(s_section_lbl_3, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_section_lbl_3, COLOR_SECTION, 0);
    lv_obj_set_style_text_font(s_section_lbl_3, info_font(), 0);

    s_content_lbl_3 = lv_label_create(container);
    lv_obj_set_width(s_content_lbl_3, LCD_WIDTH - 16);
    lv_label_set_long_mode(s_content_lbl_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_content_lbl_3, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_content_lbl_3, info_font(), 0);

    screen_info_refresh();

    return screen;
}
