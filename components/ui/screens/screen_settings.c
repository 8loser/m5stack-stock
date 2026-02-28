#include "ui_manager.h"
#include "ai_provider.h"
#include "storage.h"
#include "scheduler.h"
#include "app_config.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static lv_obj_t *s_screen              = NULL;
static lv_obj_t *s_provider_dd         = NULL;
static lv_obj_t *s_apikey_ta           = NULL;
static lv_obj_t *s_remote_token_url_ta = NULL;  /* 遠端 Token 設定 URL（開機自動抓取）*/
static lv_obj_t *s_prompt_url_ta       = NULL;  /* 遠端 Prompt URL */
static lv_obj_t *s_stocks_ta           = NULL;
static lv_obj_t *s_status_lbl          = NULL;
static lv_obj_t *s_kb                  = NULL;

static void btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_DASHBOARD);
}

static void btn_save_all_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    /* AI Provider */
    uint16_t prov_idx = lv_dropdown_get_selected(s_provider_dd);
    ai_provider_set_type((ai_provider_type_t)prov_idx);

    /* API Key（備用，遠端 Token URL 優先）*/
    const char *api_key = lv_textarea_get_text(s_apikey_ta);
    storage_ai_save_key(api_key);

    /* 遠端 Token 設定 URL（開機自動抓取 provider + api_key）*/
    const char *token_url = lv_textarea_get_text(s_remote_token_url_ta);
    storage_ai_save_remote_cfg_url(token_url);
    if (strlen(token_url) > 0) {
        lv_label_set_text(s_status_lbl, "立即抓取 Token 設定...");
        esp_err_t ret = ai_provider_fetch_remote_token_config(token_url);
        if (ret == ESP_OK) {
            lv_label_set_text(s_status_lbl, "Token 設定載入成功！");
        } else {
            lv_label_set_text(s_status_lbl, "Token URL 失敗（用本地設定）");
        }
    }

    /* 遠端 Prompt URL */
    const char *prompt_url = lv_textarea_get_text(s_prompt_url_ta);
    storage_ai_save_prompt_url(prompt_url);

    /* 若 URL 非空則立即嘗試下載 */
    if (strlen(prompt_url) > 0) {
        lv_label_set_text(s_status_lbl, "下載遠端 Prompt...");
        esp_err_t ret = ai_provider_fetch_remote_prompt(prompt_url);
        if (ret == ESP_OK) {
            const remote_prompt_config_t *rp = ai_provider_get_remote_prompt();

            /* 若遠端 Prompt 有指定額外股票，自動加入監控清單 */
            if (rp->extra_stock_count > 0) {
                stock_list_t list;
                storage_stocks_load(&list);
                for (int i = 0; i < rp->extra_stock_count; i++) {
                    bool exists = false;
                    for (int j = 0; j < list.count; j++) {
                        if (strcmp(list.symbols[j], rp->extra_stocks[i]) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists && list.count < MAX_STOCK_COUNT) {
                        strncpy(list.symbols[list.count++],
                                rp->extra_stocks[i], 7);
                    }
                }
                storage_stocks_save(&list);
            }
            lv_label_set_text(s_status_lbl, "Prompt 載入成功！");
        } else {
            lv_label_set_text(s_status_lbl, "Prompt 下載失敗（用預設）");
        }
    }

    /* 股票清單：以逗號分隔 "2330,2317,2454" */
    const char *stocks_str = lv_textarea_get_text(s_stocks_ta);
    if (strlen(stocks_str) > 0) {
        stock_list_t list = {0};
        char buf[64];
        strncpy(buf, stocks_str, sizeof(buf) - 1);
        char *token = strtok(buf, ",");
        while (token && list.count < MAX_STOCK_COUNT) {
            /* 去除空白 */
            while (*token == ' ') token++;
            if (strlen(token) >= 4) {
                strncpy(list.symbols[list.count++], token, 7);
            }
            token = strtok(NULL, ",");
        }
        storage_stocks_save(&list);
    }

    lv_label_set_text(s_status_lbl, "所有設定已儲存！");
}

static void btn_wifi_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_WIFI);
}

static void btn_schedule_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_SCHEDULE);
}

static void ta_focused_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_kb, lv_event_get_target(e));
        lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *screen_settings_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 滾動容器 */
    lv_obj_t *cont = lv_obj_create(s_screen);
    lv_obj_set_size(cont, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 6, 0);

    /* ---- 頂部列 ---- */
    lv_obj_t *topbar = lv_obj_create(cont);
    lv_obj_set_size(topbar, LCD_WIDTH, 40);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(0x0F3460), 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_back = lv_btn_create(topbar);
    lv_obj_set_size(btn_back, 55, 28);
    lv_obj_set_pos(btn_back, 4, 6);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2C3E50), 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_b = lv_label_create(btn_back);
    lv_label_set_text(lbl_b, "< 返回");
    lv_obj_center(lbl_b);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);
    lv_label_set_text(title, "系統設定");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* ---- AI Provider 下拉 ---- */
    lv_obj_t *prov_row = lv_obj_create(cont);
    lv_obj_set_size(prov_row, LCD_WIDTH - 8, 40);
    lv_obj_set_style_bg_color(prov_row, lv_color_hex(0x16213E), 0);
    lv_obj_clear_flag(prov_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *prov_lbl = lv_label_create(prov_row);
    lv_obj_set_pos(prov_lbl, 4, 10);
    lv_label_set_text(prov_lbl, "AI Provider:");
    lv_obj_set_style_text_color(prov_lbl, lv_color_hex(0xCCCCCC), 0);

    s_provider_dd = lv_dropdown_create(prov_row);
    lv_dropdown_set_options(s_provider_dd, "Gemini\nClaude\nOpenAI");
    lv_obj_set_size(s_provider_dd, 120, 32);
    lv_obj_align(s_provider_dd, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_dropdown_set_selected(s_provider_dd, (uint16_t)ai_provider_get_type());

    /* ---- API Key ---- */
    lv_obj_t *key_lbl = lv_label_create(cont);
    lv_label_set_text(key_lbl, "API Key:");
    lv_obj_set_style_text_color(key_lbl, lv_color_hex(0xCCCCCC), 0);

    s_apikey_ta = lv_textarea_create(cont);
    lv_obj_set_size(s_apikey_ta, LCD_WIDTH - 8, 34);
    lv_textarea_set_one_line(s_apikey_ta, true);
    lv_textarea_set_password_mode(s_apikey_ta, true);
    lv_textarea_set_placeholder_text(s_apikey_ta, "貼上 API Key");
    lv_obj_add_event_cb(s_apikey_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    /* 載入已儲存的 Key */
    char api_key[128] = {0};
    storage_ai_load_key(api_key, sizeof(api_key));
    if (strlen(api_key) > 0) lv_textarea_set_text(s_apikey_ta, api_key);

    /* ---- 遠端 Token 設定 URL（開機自動抓取）---- */
    lv_obj_t *token_url_lbl = lv_label_create(cont);
    lv_label_set_text(token_url_lbl, "遠端 Token 設定 URL:");
    lv_obj_set_style_text_color(token_url_lbl, lv_color_hex(0xCCCCCC), 0);

    lv_obj_t *token_url_hint = lv_label_create(cont);
    lv_label_set_text(token_url_hint, "(開機自動抓取 provider + api_key，擇一使用)");
    lv_obj_set_style_text_color(token_url_hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(token_url_hint, &lv_font_montserrat_10, 0);

    s_remote_token_url_ta = lv_textarea_create(cont);
    lv_obj_set_size(s_remote_token_url_ta, LCD_WIDTH - 8, 34);
    lv_textarea_set_one_line(s_remote_token_url_ta, true);
    lv_textarea_set_placeholder_text(s_remote_token_url_ta,
                                     "https://gist.githubusercontent.com/user/id/raw/token.json");
    lv_obj_add_event_cb(s_remote_token_url_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    char remote_cfg_url[256] = {0};
    storage_ai_load_remote_cfg_url(remote_cfg_url, sizeof(remote_cfg_url));
    if (strlen(remote_cfg_url) > 0)
        lv_textarea_set_text(s_remote_token_url_ta, remote_cfg_url);

    /* ---- 遠端 Prompt URL ---- */
    lv_obj_t *url_lbl = lv_label_create(cont);
    lv_label_set_text(url_lbl, "遠端 Prompt URL:");
    lv_obj_set_style_text_color(url_lbl, lv_color_hex(0xCCCCCC), 0);

    lv_obj_t *url_hint = lv_label_create(cont);
    lv_label_set_text(url_hint, "(JSON 格式，指定分析股票與訊號來源)");
    lv_obj_set_style_text_color(url_hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(url_hint, &lv_font_montserrat_10, 0);

    s_prompt_url_ta = lv_textarea_create(cont);
    lv_obj_set_size(s_prompt_url_ta, LCD_WIDTH - 8, 34);
    lv_textarea_set_one_line(s_prompt_url_ta, true);
    lv_textarea_set_placeholder_text(s_prompt_url_ta, "https://example.com/prompt.json");
    lv_obj_add_event_cb(s_prompt_url_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    char prompt_url[256] = {0};
    storage_ai_load_prompt_url(prompt_url, sizeof(prompt_url));
    if (strlen(prompt_url) > 0) lv_textarea_set_text(s_prompt_url_ta, prompt_url);

    /* ---- 股票清單 ---- */
    lv_obj_t *stocks_lbl = lv_label_create(cont);
    lv_label_set_text(stocks_lbl, "監控股票（逗號分隔）:");
    lv_obj_set_style_text_color(stocks_lbl, lv_color_hex(0xCCCCCC), 0);

    s_stocks_ta = lv_textarea_create(cont);
    lv_obj_set_size(s_stocks_ta, LCD_WIDTH - 8, 34);
    lv_textarea_set_one_line(s_stocks_ta, true);
    lv_textarea_set_placeholder_text(s_stocks_ta, "2330,2317,2454,2412");
    lv_obj_add_event_cb(s_stocks_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    stock_list_t stock_list;
    storage_stocks_load(&stock_list);
    char stocks_str[64] = {0};
    for (int i = 0; i < stock_list.count; i++) {
        if (i > 0) strcat(stocks_str, ",");
        strcat(stocks_str, stock_list.symbols[i]);
    }
    lv_textarea_set_text(s_stocks_ta, stocks_str);

    /* ---- 快捷按鈕 ---- */
    lv_obj_t *btn_row = lv_obj_create(cont);
    lv_obj_set_size(btn_row, LCD_WIDTH - 8, 36);
    lv_obj_set_style_bg_color(btn_row, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_pad_color(btn_row, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_wifi = lv_btn_create(btn_row);
    lv_obj_set_size(btn_wifi, 90, 28);
    lv_obj_set_style_bg_color(btn_wifi, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_wifi, btn_wifi_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(lbl_wifi, "WiFi設定");
    lv_obj_center(lbl_wifi);

    lv_obj_t *btn_sched = lv_btn_create(btn_row);
    lv_obj_set_size(btn_sched, 90, 28);
    lv_obj_set_style_bg_color(btn_sched, lv_color_hex(0x8E44AD), 0);
    lv_obj_add_event_cb(btn_sched, btn_schedule_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_sched = lv_label_create(btn_sched);
    lv_label_set_text(lbl_sched, "排程設定");
    lv_obj_center(lbl_sched);

    lv_obj_t *btn_save = lv_btn_create(btn_row);
    lv_obj_set_size(btn_save, 90, 28);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x27AE60), 0);
    lv_obj_add_event_cb(btn_save, btn_save_all_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "儲存全部");
    lv_obj_center(lbl_save);

    /* 狀態訊息 */
    s_status_lbl = lv_label_create(cont);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x44BB44), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_10, 0);

    /* 鍵盤（預設隱藏）*/
    s_kb = lv_keyboard_create(s_screen);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);

    return s_screen;
}
