#include "ui_manager.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static void ap_item_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t *ssid_ta = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *b   = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(b, 1);
    if (!lbl || !ssid_ta) return;
    const char *txt = lv_label_get_text(lbl);
    char ssid[33] = {0};
    const char *paren = strchr(txt, '(');
    size_t len = paren ? (size_t)(paren - txt - 1) : strlen(txt);
    if (len > 0 && len < 33) {
        memcpy(ssid, txt, len);
        lv_textarea_set_text(ssid_ta, ssid);
    }
}

static lv_obj_t *s_screen       = NULL;
static lv_obj_t *s_ap_list      = NULL;
static lv_obj_t *s_ssid_ta      = NULL;
static lv_obj_t *s_pw_ta        = NULL;
static lv_obj_t *s_status_lbl   = NULL;
static lv_obj_t *s_kb           = NULL;

static void btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_DASHBOARD);
}

static void btn_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_label_set_text(s_status_lbl, "掃描中...");

    wifi_ap_info_t aps[10];
    uint16_t count = 0;
    wifi_manager_scan(aps, &count, 10);

    lv_list_clean(s_ap_list);
    for (int i = 0; i < count; i++) {
        char item_text[50];
        snprintf(item_text, sizeof(item_text), "%s (%ddBm)",
                 aps[i].ssid, aps[i].rssi);
        lv_obj_t *btn = lv_list_add_btn(s_ap_list, LV_SYMBOL_WIFI, item_text);
        lv_obj_add_event_cb(btn, ap_item_click_cb, LV_EVENT_CLICKED, s_ssid_ta);
    }

    char msg[32];
    snprintf(msg, sizeof(msg), "找到 %d 個 AP", count);
    lv_label_set_text(s_status_lbl, msg);
}

static void btn_connect_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pw   = lv_textarea_get_text(s_pw_ta);

    if (!ssid || strlen(ssid) == 0) {
        lv_label_set_text(s_status_lbl, "請輸入 SSID");
        return;
    }

    lv_label_set_text(s_status_lbl, "連線中...");
    esp_err_t ret = wifi_manager_connect(ssid, pw);
    if (ret == ESP_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "已連線 IP:%s", wifi_manager_get_ip());
        lv_label_set_text(s_status_lbl, msg);
    } else {
        lv_label_set_text(s_status_lbl, "連線失敗，請重試");
    }
}

static void ta_focused_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_kb, lv_event_get_target(e));
        lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *screen_wifi_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 頂部列 */
    lv_obj_t *topbar = lv_obj_create(s_screen);
    lv_obj_set_size(topbar, LCD_WIDTH, 40);
    lv_obj_set_pos(topbar, 0, 0);
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
    lv_label_set_text(title, "WiFi 設定");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* AP 掃描列表 */
    s_ap_list = lv_list_create(s_screen);
    lv_obj_set_size(s_ap_list, LCD_WIDTH - 8, 80);
    lv_obj_set_pos(s_ap_list, 4, 44);

    /* 掃描按鈕 */
    lv_obj_t *btn_scan = lv_btn_create(s_screen);
    lv_obj_set_size(btn_scan, 80, 28);
    lv_obj_set_pos(btn_scan, 230, 44);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_scan, btn_scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "掃描");
    lv_obj_center(lbl_scan);

    /* SSID 輸入 */
    lv_obj_t *ssid_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(ssid_lbl, 4, 130);
    lv_label_set_text(ssid_lbl, "SSID:");
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0xCCCCCC), 0);

    s_ssid_ta = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ssid_ta, 200, 32);
    lv_obj_set_pos(s_ssid_ta, 60, 126);
    lv_textarea_set_one_line(s_ssid_ta, true);
    lv_textarea_set_placeholder_text(s_ssid_ta, "WiFi 名稱");
    lv_obj_add_event_cb(s_ssid_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    /* 密碼輸入 */
    lv_obj_t *pw_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(pw_lbl, 4, 164);
    lv_label_set_text(pw_lbl, "密碼:");
    lv_obj_set_style_text_color(pw_lbl, lv_color_hex(0xCCCCCC), 0);

    s_pw_ta = lv_textarea_create(s_screen);
    lv_obj_set_size(s_pw_ta, 200, 32);
    lv_obj_set_pos(s_pw_ta, 60, 160);
    lv_textarea_set_one_line(s_pw_ta, true);
    lv_textarea_set_password_mode(s_pw_ta, true);
    lv_textarea_set_placeholder_text(s_pw_ta, "WiFi 密碼");
    lv_obj_add_event_cb(s_pw_ta, ta_focused_cb, LV_EVENT_FOCUSED, NULL);

    /* 連線按鈕 */
    lv_obj_t *btn_conn = lv_btn_create(s_screen);
    lv_obj_set_size(btn_conn, 90, 28);
    lv_obj_set_pos(btn_conn, 115, 198);
    lv_obj_set_style_bg_color(btn_conn, lv_color_hex(0x27AE60), 0);
    lv_obj_add_event_cb(btn_conn, btn_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_conn = lv_label_create(btn_conn);
    lv_label_set_text(lbl_conn, "連線");
    lv_obj_center(lbl_conn);

    /* 狀態 */
    s_status_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_status_lbl, 4, 232);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_10, 0);

    /* 鍵盤（預設隱藏）*/
    s_kb = lv_keyboard_create(s_screen);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);

    return s_screen;
}
