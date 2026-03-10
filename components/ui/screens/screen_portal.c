#include "screen_portal.h"
#include "network_portal.h"
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
static lv_obj_t *s_qr_area    = NULL;
static lv_obj_t *s_qr_obj      = NULL;
static lv_obj_t *s_qr_hint_lbl = NULL;
static lv_obj_t *s_ap_ssid_hdr_lbl = NULL;
static lv_obj_t *s_ap_ssid_lbl = NULL;
static lv_obj_t *s_ap_pwd_hdr_lbl = NULL;
static lv_obj_t *s_ap_pwd_lbl = NULL;
static lv_obj_t *s_ap_ip_hdr_lbl = NULL;
static lv_obj_t *s_ap_ip_lbl   = NULL;
static lv_obj_t *s_connected_ap_hdr_lbl = NULL;
static lv_obj_t *s_connected_ap_lbl = NULL;
static lv_obj_t *s_sta_ip_hdr_lbl = NULL;
static lv_obj_t *s_sta_ip_lbl  = NULL;
static const char *TAG = "screen_portal";

static void set_obj_visible(lv_obj_t *obj, bool visible)
{
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool is_softap_enabled(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }

    return (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
}

static void refresh_network_info_labels(void)
{
    bool sta_connected = (network_portal_get_state() == WIFI_STATE_CONNECTED);
    bool portal_active = network_portal_is_provisioning_portal_active();
    bool portal_sta_mode = portal_active && network_portal_is_portal_sta_mode();
    bool portal_ap_mode = portal_active && !portal_sta_mode;
    bool show_ap_info = portal_ap_mode || is_softap_enabled();
    const char *connected_ssid = "--";
    const char *sta_ip = "--";
    const char *ap_ip = "--";

    if (sta_connected) {
        const char *ssid = network_portal_get_connected_ssid();
        const char *ip = network_portal_get_ip();
        if (ssid && ssid[0] != '\0') {
            connected_ssid = ssid;
        }
        if (ip && ip[0] != '\0') {
            sta_ip = ip;
        }
    }

    if (show_ap_info) {
        const char *ip = network_portal_get_provisioning_ap_ip();
        if (ip && ip[0] != '\0') {
            ap_ip = ip;
        }
    }

    if (s_ap_ssid_lbl) {
        lv_label_set_text(s_ap_ssid_lbl, network_portal_get_provisioning_ap_ssid());
    }
    if (s_ap_pwd_lbl) {
        lv_label_set_text(s_ap_pwd_lbl, network_portal_get_provisioning_ap_password());
    }
    if (s_sta_ip_lbl) {
        lv_label_set_text(s_sta_ip_lbl, sta_ip);
    }
    if (s_ap_ip_lbl) {
        lv_label_set_text(s_ap_ip_lbl, ap_ip);
    }
    if (s_connected_ap_lbl) {
        lv_label_set_text(s_connected_ap_lbl, connected_ssid);
    }
}

static void update_portal_ui(bool active)
{
    bool sta_mode = active && network_portal_is_portal_sta_mode();
    bool ap_mode = active && !sta_mode;

    if (sta_mode) {
        /* STA mode: hide QR and show connected AP + LAN IP. */
        set_obj_visible(s_qr_obj, false);
        if (s_qr_hint_lbl) {
            lv_label_set_text(s_qr_hint_lbl,
                              "已連上 WiFi\n\n請看右側\n連線資訊");
        }
        set_obj_visible(s_qr_hint_lbl, true);
    } else if (ap_mode) {
        /* AP/APSTA provisioning mode: show WiFi QR and AP details. */
        set_obj_visible(s_qr_obj, true);
        set_obj_visible(s_qr_hint_lbl, false);
    } else {
        set_obj_visible(s_qr_obj, false);
        if (s_qr_hint_lbl) {
            lv_label_set_text(s_qr_hint_lbl,
                              "入口未啟動\n\n等待自動啟動，\n再掃描 QR。");
        }
        set_obj_visible(s_qr_hint_lbl, true);
    }

    set_obj_visible(s_ap_ssid_hdr_lbl, ap_mode);
    set_obj_visible(s_ap_ssid_lbl, ap_mode);
    set_obj_visible(s_ap_pwd_hdr_lbl, ap_mode);
    set_obj_visible(s_ap_pwd_lbl, ap_mode);
    set_obj_visible(s_ap_ip_hdr_lbl, ap_mode);
    set_obj_visible(s_ap_ip_lbl, ap_mode);

    set_obj_visible(s_connected_ap_hdr_lbl, sta_mode);
    set_obj_visible(s_connected_ap_lbl, sta_mode);
    set_obj_visible(s_sta_ip_hdr_lbl, sta_mode);
    set_obj_visible(s_sta_ip_lbl, sta_mode);

    refresh_network_info_labels();
}

void screen_portal_refresh_network_info(void)
{
    refresh_network_info_labels();
}

void screen_portal_open_portal(void)
{
    ESP_LOGI(TAG, "[%u ms] open_portal begin active=%d softap=%d",
             (unsigned)esp_log_timestamp(),
             (int)network_portal_is_provisioning_portal_active(),
             (int)is_softap_enabled());
    if (network_portal_is_provisioning_portal_active()) {
        update_portal_ui(true);
        ESP_LOGI(TAG, "[%u ms] open_portal skip (already active)",
                 (unsigned)esp_log_timestamp());
        return;
    }

    esp_err_t ret = network_portal_start_provisioning_portal();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[%u ms] open_portal failed: %s",
                 (unsigned)esp_log_timestamp(), esp_err_to_name(ret));
        update_portal_ui(false);
    } else {
        update_portal_ui(true);
        ESP_LOGI(TAG, "[%u ms] open_portal done",
                 (unsigned)esp_log_timestamp());
    }
}

void screen_portal_close_portal(void)
{
    ESP_LOGI(TAG, "[%u ms] close_portal begin active=%d softap=%d",
             (unsigned)esp_log_timestamp(),
             (int)network_portal_is_provisioning_portal_active(),
             (int)is_softap_enabled());
    if (!network_portal_is_provisioning_portal_active() && !is_softap_enabled()) {
        ESP_LOGI(TAG, "[%u ms] close_portal skip (already inactive)",
                 (unsigned)esp_log_timestamp());
        return;
    }

    esp_err_t ret = network_portal_stop_provisioning_portal();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[%u ms] close_portal failed: %s",
                 (unsigned)esp_log_timestamp(), esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "[%u ms] close_portal done",
                 (unsigned)esp_log_timestamp());
    }
}

lv_obj_t *screen_portal_create(void)
{
    const lv_coord_t content_top = UI_CONTENT_TOP_Y + 16;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 主要內容區：固定顯示，避免底部空白 */
    s_qr_area = lv_obj_create(s_screen);
    lv_obj_set_size(s_qr_area, LCD_WIDTH, LCD_HEIGHT - content_top);
    lv_obj_set_pos(s_qr_area, 0, content_top);
    lv_obj_set_style_bg_opa(s_qr_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_qr_area, 0, 0);
    lv_obj_set_style_pad_all(s_qr_area, 0, 0);
    lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_SCROLLABLE);

    s_qr_hint_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_qr_hint_lbl, 4, 2);
    lv_obj_set_size(s_qr_hint_lbl, 156, 196);
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
             network_portal_get_provisioning_ap_ssid(),
             network_portal_get_provisioning_ap_password());
    s_qr_obj = lv_qrcode_create(s_qr_area, 146,
                                lv_color_hex(0x000000),
                                lv_color_hex(0xFFFFFF));
    lv_qrcode_update(s_qr_obj, portal_ap_qr_str, strlen(portal_ap_qr_str));
    lv_obj_set_pos(s_qr_obj, 13, 6);
#else
    s_qr_obj = lv_label_create(s_qr_area);
    lv_obj_center(s_qr_obj);
    lv_label_set_text(s_qr_obj, "QR 未啟用\n請手動輸入 SSID");
    lv_obj_set_style_text_align(s_qr_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_qr_obj, lv_color_hex(0xFFCC66), 0);
    lv_obj_set_style_text_font(s_qr_obj, &lv_font_noto_tc_14, 0);
#endif

    /* 右側說明資訊 */
    s_ap_ssid_hdr_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_ssid_hdr_lbl, 176, 8);
    lv_label_set_text(s_ap_ssid_hdr_lbl, "加入 AP:");
    lv_obj_set_style_text_color(s_ap_ssid_hdr_lbl, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(s_ap_ssid_hdr_lbl, &lv_font_noto_tc_14, 0);

    s_ap_ssid_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_ssid_lbl, 176, 22);
    lv_obj_set_width(s_ap_ssid_lbl, 138);
    lv_label_set_long_mode(s_ap_ssid_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_ap_ssid_lbl, network_portal_get_provisioning_ap_ssid());
    lv_obj_set_style_text_color(s_ap_ssid_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_ap_ssid_lbl, &lv_font_noto_tc_14, 0);

    s_ap_pwd_hdr_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_pwd_hdr_lbl, 176, 50);
    lv_label_set_text(s_ap_pwd_hdr_lbl, "密碼:");
    lv_obj_set_style_text_color(s_ap_pwd_hdr_lbl, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(s_ap_pwd_hdr_lbl, &lv_font_noto_tc_14, 0);

    s_ap_pwd_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_pwd_lbl, 176, 64);
    lv_label_set_text(s_ap_pwd_lbl, network_portal_get_provisioning_ap_password());
    lv_obj_set_style_text_color(s_ap_pwd_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_ap_pwd_lbl, &lv_font_noto_tc_14, 0);

    s_ap_ip_hdr_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_ip_hdr_lbl, 176, 92);
    lv_label_set_text(s_ap_ip_hdr_lbl, "配網 IP:");
    lv_obj_set_style_text_color(s_ap_ip_hdr_lbl, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(s_ap_ip_hdr_lbl, &lv_font_noto_tc_14, 0);

    s_ap_ip_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_ap_ip_lbl, 176, 106);
    lv_label_set_text(s_ap_ip_lbl, "--");
    lv_obj_set_style_text_color(s_ap_ip_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_ap_ip_lbl, &lv_font_noto_tc_14, 0);

    s_connected_ap_hdr_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_connected_ap_hdr_lbl, 176, 8);
    lv_label_set_text(s_connected_ap_hdr_lbl, "連線 AP:");
    lv_obj_set_style_text_color(s_connected_ap_hdr_lbl, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(s_connected_ap_hdr_lbl, &lv_font_noto_tc_14, 0);

    s_connected_ap_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_connected_ap_lbl, 176, 22);
    lv_obj_set_width(s_connected_ap_lbl, 138);
    lv_label_set_long_mode(s_connected_ap_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_connected_ap_lbl, "--");
    lv_obj_set_style_text_color(s_connected_ap_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_connected_ap_lbl, &lv_font_noto_tc_14, 0);

    s_sta_ip_hdr_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_sta_ip_hdr_lbl, 176, 50);
    lv_label_set_text(s_sta_ip_hdr_lbl, "內網 IP:");
    lv_obj_set_style_text_color(s_sta_ip_hdr_lbl, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(s_sta_ip_hdr_lbl, &lv_font_noto_tc_14, 0);

    s_sta_ip_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(s_sta_ip_lbl, 176, 64);
    lv_label_set_text(s_sta_ip_lbl, "--");
    lv_obj_set_style_text_color(s_sta_ip_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_sta_ip_lbl, &lv_font_noto_tc_14, 0);

    /* 根據目前 Portal 狀態初始化 UI */
    update_portal_ui(network_portal_is_provisioning_portal_active());

    return s_screen;
}
