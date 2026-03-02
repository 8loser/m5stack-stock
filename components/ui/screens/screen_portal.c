#include "ui_manager.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "ui_compat.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdio.h>

#if LV_USE_QRCODE
#include "extra/libs/qrcode/lv_qrcode.h"
#endif

static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_status_lbl = NULL;
static lv_obj_t *s_qr_area    = NULL;
static lv_obj_t *s_qr_obj      = NULL;
static lv_obj_t *s_qr_hint_lbl = NULL;
static const char *TAG = "screen_portal";

static bool is_softap_enabled(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }

    return (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
}

static void update_portal_ui(bool active)
{
    if (active) {
        lv_label_set_text(s_status_lbl, "入口已啟動 - 掃描 QR 加入 AP");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x4CAF50), 0);
        if (s_qr_obj) {
            lv_obj_clear_flag(s_qr_obj, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_qr_hint_lbl) {
            lv_obj_add_flag(s_qr_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_label_set_text(s_status_lbl, "入口未啟動 - 等待自動啟動");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
        if (s_qr_obj) {
            lv_obj_add_flag(s_qr_obj, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_qr_hint_lbl) {
            lv_obj_clear_flag(s_qr_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void screen_portal_open_portal(void)
{
    if (!wifi_manager_is_provisioning_portal_active()) {
        esp_err_t ret = wifi_manager_start_provisioning_portal();
        update_portal_ui(ret == ESP_OK);
    } else {
        update_portal_ui(true);
    }
}

void screen_portal_close_portal(void)
{
    if (!wifi_manager_is_provisioning_portal_active() &&
        !is_softap_enabled()) {
        update_portal_ui(false);
        return;
    }

    esp_err_t ret = wifi_manager_stop_provisioning_portal();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "停止 portal 失敗: %s", esp_err_to_name(ret));
    }

    /* 二次檢查 SoftAP/portal 是否已停止；若仍啟用，再嘗試一次停止 */
    if (wifi_manager_is_provisioning_portal_active() ||
        is_softap_enabled()) {
        ESP_LOGW(TAG, "偵測到 portal/AP 仍啟用，進行第二次停止");
        ret = wifi_manager_stop_provisioning_portal();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "第二次停止 portal 失敗: %s", esp_err_to_name(ret));
        }
    }

    update_portal_ui(wifi_manager_is_provisioning_portal_active() ||
                     is_softap_enabled());
}

lv_obj_t *screen_portal_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 狀態訊息 */
    s_status_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_status_lbl, 4, 22);
    lv_obj_set_width(s_status_lbl, LCD_WIDTH - 8);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_status_lbl, "入口未啟動");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_noto_tc_14, 0);

    /* 主要內容區：固定顯示，避免底部空白 */
    s_qr_area = lv_obj_create(s_screen);
    lv_obj_set_size(s_qr_area, LCD_WIDTH, LCD_HEIGHT - 38);
    lv_obj_set_pos(s_qr_area, 0, 38);
    lv_obj_set_style_bg_opa(s_qr_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_qr_area, 0, 0);
    lv_obj_set_style_pad_all(s_qr_area, 0, 0);
    lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_panel = lv_obj_create(s_qr_area);
    lv_obj_set_size(left_panel, 164, 196);
    lv_obj_set_pos(left_panel, 4, 2);
    lv_obj_set_style_radius(left_panel, 6, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x0F3460), 0);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_40, 0);
    lv_obj_set_style_border_width(left_panel, 1, 0);
    lv_obj_set_style_border_color(left_panel, lv_color_hex(0x2A4A70), 0);
    lv_obj_set_style_pad_all(left_panel, 6, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_qr_hint_lbl = lv_label_create(left_panel);
    lv_obj_center(s_qr_hint_lbl);
    lv_obj_set_width(s_qr_hint_lbl, 148);
    lv_label_set_long_mode(s_qr_hint_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_qr_hint_lbl,
                      "入口未啟動\n\n等待自動啟動，\n再掃描 QR。");
    lv_obj_set_style_text_align(s_qr_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_qr_hint_lbl, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(s_qr_hint_lbl, &lv_font_noto_tc_14, 0);

#if LV_USE_QRCODE
    /* WiFi QR：掃描後手機直接加入 AP（WIFI: URI）*/
    char portal_ap_qr_str[96];
    snprintf(portal_ap_qr_str, sizeof(portal_ap_qr_str), "WIFI:T:WPA;S:%s;P:%s;;",
             wifi_manager_get_provisioning_ap_ssid(),
             wifi_manager_get_provisioning_ap_password());
    s_qr_obj = lv_qrcode_create(left_panel, 146,
                                lv_color_hex(0x000000),
                                lv_color_hex(0xFFFFFF));
    lv_qrcode_update(s_qr_obj, portal_ap_qr_str, strlen(portal_ap_qr_str));
    lv_obj_align(s_qr_obj, LV_ALIGN_TOP_MID, 0, 4);
#else
    s_qr_obj = lv_label_create(left_panel);
    lv_obj_center(s_qr_obj);
    lv_label_set_text(s_qr_obj, "QR 未啟用\n請手動輸入 SSID");
    lv_obj_set_style_text_align(s_qr_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_qr_obj, lv_color_hex(0xFFCC66), 0);
    lv_obj_set_style_text_font(s_qr_obj, &lv_font_noto_tc_14, 0);
#endif

    /* 右側說明資訊 */
    lv_obj_t *hdr_ap = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_ap, 176, 8);
    lv_label_set_text(hdr_ap, "加入 AP:");
    lv_obj_set_style_text_color(hdr_ap, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_ap, &lv_font_noto_tc_14, 0);

    lv_obj_t *ssid_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(ssid_lbl, 176, 22);
    lv_obj_set_width(ssid_lbl, 138);
    lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(ssid_lbl, wifi_manager_get_provisioning_ap_ssid());
    lv_obj_set_style_text_color(ssid_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(ssid_lbl, &lv_font_noto_tc_14, 0);

    lv_obj_t *hdr_pwd = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_pwd, 176, 50);
    lv_label_set_text(hdr_pwd, "密碼:");
    lv_obj_set_style_text_color(hdr_pwd, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_pwd, &lv_font_noto_tc_14, 0);

    lv_obj_t *pwd_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(pwd_lbl, 176, 64);
    lv_label_set_text(pwd_lbl, wifi_manager_get_provisioning_ap_password());
    lv_obj_set_style_text_color(pwd_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(pwd_lbl, &lv_font_noto_tc_14, 0);

    lv_obj_t *hdr_url = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_url, 176, 92);
    lv_label_set_text(hdr_url, "網址:");
    lv_obj_set_style_text_color(hdr_url, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_url, &lv_font_noto_tc_14, 0);

    lv_obj_t *url_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(url_lbl, 176, 106);
    lv_label_set_text(url_lbl, "192.168.4.1");
    lv_obj_set_style_text_color(url_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(url_lbl, &lv_font_noto_tc_14, 0);

    lv_obj_t *hint_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(hint_lbl, 176, 136);
    lv_obj_set_width(hint_lbl, 138);
    lv_label_set_long_mode(hint_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint_lbl, "1. 加入 AP\n2. 開啟瀏覽器\n3. 提交 WiFi");
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_noto_tc_14, 0);

    /* 根據目前 Portal 狀態初始化 UI */
    update_portal_ui(wifi_manager_is_provisioning_portal_active());

    return s_screen;
}
