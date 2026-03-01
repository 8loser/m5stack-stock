#include "ui_manager.h"
#include "wifi_manager.h"
#include "rtc_bm8563.h"
#include "app_config.h"
#include "ui_compat.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#if LV_USE_QRCODE
#include "extra/libs/qrcode/lv_qrcode.h"
#endif

static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_status_lbl = NULL;
static lv_obj_t *s_qr_area    = NULL;
static lv_obj_t *s_time_lbl   = NULL;
static lv_timer_t *s_time_timer = NULL;
static const char *TAG = "screen_wifi";

static bool is_softap_enabled(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }

    return (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
}

static void update_topbar_time(void)
{
    if (!s_time_lbl) return;

    rtc_time_t rt;
    if (rtc_bm8563_get_time(&rt) == ESP_OK &&
        rt.hours < 24 && rt.minutes < 60) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d", rt.hours, rt.minutes);
        lv_label_set_text(s_time_lbl, buf);
        return;
    }

    time_t now = time(NULL);
    if (now > 0) {
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        lv_label_set_text(s_time_lbl, buf);
        return;
    }

    lv_label_set_text(s_time_lbl, "--:--");
}

static void time_update_cb(lv_timer_t *t)
{
    (void)t;
    update_topbar_time();
}

static void update_portal_ui(bool active)
{
    if (active) {
        lv_label_set_text(s_status_lbl, "Portal active - scan QR to join AP");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x4CAF50), 0);
        lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_status_lbl, "Portal inactive");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
        lv_obj_add_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_wifi_on_btn(uint8_t btn)
{
    (void)btn;
}

void screen_wifi_open_portal(void)
{
    if (!wifi_manager_is_provisioning_portal_active()) {
        esp_err_t ret = wifi_manager_start_provisioning_portal();
        update_portal_ui(ret == ESP_OK);
    } else {
        update_portal_ui(true);
    }
}

void screen_wifi_close_portal(void)
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

lv_obj_t *screen_wifi_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 頂部列（h=20：左側時間 / 中間標題）*/
    lv_obj_t *topbar = lv_obj_create(s_screen);
    lv_obj_set_size(topbar, LCD_WIDTH, 20);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(0x0F3460), 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    s_time_lbl = lv_label_create(topbar);
    lv_obj_set_pos(s_time_lbl, 4, 2);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    if (!s_time_timer) {
        s_time_timer = lv_timer_create(time_update_cb, 10000, NULL);
    }
    update_topbar_time();

    /* 狀態訊息 */
    s_status_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_status_lbl, 4, 22);
    lv_obj_set_width(s_status_lbl, LCD_WIDTH - 8);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_status_lbl, "Portal inactive");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_10, 0);

    /* QR 區域容器（portal 未啟用時隱藏）*/
    s_qr_area = lv_obj_create(s_screen);
    lv_obj_set_size(s_qr_area, LCD_WIDTH, 164);
    lv_obj_set_pos(s_qr_area, 0, 38);
    lv_obj_set_style_bg_opa(s_qr_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_qr_area, 0, 0);
    lv_obj_set_style_pad_all(s_qr_area, 0, 0);
    lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);

#if LV_USE_QRCODE
    /* WiFi QR：掃描後手機直接加入 AP（WIFI: URI）*/
    char wifi_qr_str[96];
    snprintf(wifi_qr_str, sizeof(wifi_qr_str), "WIFI:T:WPA;S:%s;P:%s;;",
             wifi_manager_get_provisioning_ap_ssid(),
             wifi_manager_get_provisioning_ap_password());
    lv_obj_t *qr = lv_qrcode_create(s_qr_area, 156,
                                     lv_color_hex(0x000000),
                                     lv_color_hex(0xFFFFFF));
    lv_qrcode_update(qr, wifi_qr_str, strlen(wifi_qr_str));
    lv_obj_set_pos(qr, 4, 4);
#else
    lv_obj_t *qr_fallback = lv_label_create(s_qr_area);
    lv_obj_set_pos(qr_fallback, 4, 4);
    lv_label_set_text(qr_fallback, "QR disabled\nenter SSID manually");
    lv_obj_set_style_text_color(qr_fallback, lv_color_hex(0xFFCC66), 0);
    lv_obj_set_style_text_font(qr_fallback, &lv_font_montserrat_10, 0);
#endif

    /* 右側說明資訊 */
    lv_obj_t *hdr_ap = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_ap, 166, 4);
    lv_label_set_text(hdr_ap, "Join AP:");
    lv_obj_set_style_text_color(hdr_ap, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_ap, &lv_font_montserrat_10, 0);

    lv_obj_t *ssid_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(ssid_lbl, 166, 18);
    lv_label_set_text(ssid_lbl, wifi_manager_get_provisioning_ap_ssid());
    lv_obj_set_style_text_color(ssid_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *hdr_pwd = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_pwd, 166, 38);
    lv_label_set_text(hdr_pwd, "Pwd:");
    lv_obj_set_style_text_color(hdr_pwd, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_pwd, &lv_font_montserrat_10, 0);

    lv_obj_t *pwd_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(pwd_lbl, 166, 52);
    lv_label_set_text(pwd_lbl, wifi_manager_get_provisioning_ap_password());
    lv_obj_set_style_text_color(pwd_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(pwd_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *hdr_url = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr_url, 166, 78);
    lv_label_set_text(hdr_url, "URL:");
    lv_obj_set_style_text_color(hdr_url, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr_url, &lv_font_montserrat_10, 0);

    lv_obj_t *url_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(url_lbl, 166, 92);
    lv_label_set_text(url_lbl, "192.168.4.1");
    lv_obj_set_style_text_color(url_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *hint_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(hint_lbl, 166, 112);
    lv_obj_set_width(hint_lbl, 148);
    lv_label_set_long_mode(hint_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint_lbl, "Auto-opens in browser");
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_montserrat_10, 0);

    /* 根據目前 Portal 狀態初始化 UI */
    update_portal_ui(wifi_manager_is_provisioning_portal_active());

    return s_screen;
}
