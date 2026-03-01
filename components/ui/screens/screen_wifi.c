#include "ui_manager.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "ui_compat.h"
#include <string.h>
#include <stdio.h>

#if LV_USE_QRCODE
#include "extra/libs/qrcode/lv_qrcode.h"
#endif

static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_status_lbl = NULL;
static lv_obj_t *s_qr_area    = NULL;
static lv_obj_t *s_btn_portal = NULL;

static void update_portal_ui(bool active)
{
    if (active) {
        lv_label_set_text(s_status_lbl, "Portal active - scan QR to configure");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x4CAF50), 0);
        lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *lbl = lv_obj_get_child(s_btn_portal, 0);
        if (lbl) lv_label_set_text(lbl, "Stop Portal");
        lv_obj_set_style_bg_color(s_btn_portal, lv_color_hex(0xE74C3C), 0);
    } else {
        lv_label_set_text(s_status_lbl, "Portal inactive");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
        lv_obj_add_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *lbl = lv_obj_get_child(s_btn_portal, 0);
        if (lbl) lv_label_set_text(lbl, "Start Portal");
        lv_obj_set_style_bg_color(s_btn_portal, lv_color_hex(0x8E44AD), 0);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_switch_screen(SCREEN_DASHBOARD);
    }
}

static void btn_toggle_portal_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (wifi_manager_is_provisioning_portal_active()) {
        wifi_manager_stop_provisioning_portal();
        update_portal_ui(false);
    } else {
        esp_err_t ret = wifi_manager_start_provisioning_portal();
        update_portal_ui(ret == ESP_OK);
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
    lv_label_set_text(lbl_b, "< Back");
    lv_obj_center(lbl_b);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* 狀態列 */
    s_status_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_status_lbl, 4, 44);
    lv_obj_set_width(s_status_lbl, LCD_WIDTH - 8);
    lv_label_set_long_mode(s_status_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_status_lbl, "Portal inactive");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_10, 0);

    /* QR 區域容器（portal 未啟用時隱藏） */
    s_qr_area = lv_obj_create(s_screen);
    lv_obj_set_size(s_qr_area, LCD_WIDTH, 136);
    lv_obj_set_pos(s_qr_area, 0, 62);
    lv_obj_set_style_bg_opa(s_qr_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_qr_area, 0, 0);
    lv_obj_set_style_pad_all(s_qr_area, 0, 0);
    lv_obj_clear_flag(s_qr_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_qr_area, LV_OBJ_FLAG_HIDDEN);

    /* Step 1 標題 */
    lv_obj_t *hdr1 = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr1, 15, 0);
    lv_label_set_text(hdr1, "1: Join AP");
    lv_obj_set_style_text_color(hdr1, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr1, &lv_font_montserrat_10, 0);

    /* Step 2 標題 */
    lv_obj_t *hdr2 = lv_label_create(s_qr_area);
    lv_obj_set_pos(hdr2, 170, 0);
    lv_label_set_text(hdr2, "2: Open URL");
    lv_obj_set_style_text_color(hdr2, lv_color_hex(0x64B5F6), 0);
    lv_obj_set_style_text_font(hdr2, &lv_font_montserrat_10, 0);

#if LV_USE_QRCODE
    /* WiFi 連線 QR：手機掃描後直接加入 AP */
    char wifi_qr_str[96];
    snprintf(wifi_qr_str, sizeof(wifi_qr_str), "WIFI:T:WPA;S:%s;P:%s;;",
             wifi_manager_get_provisioning_ap_ssid(),
             wifi_manager_get_provisioning_ap_password());
    lv_obj_t *qr_wifi = lv_qrcode_create(s_qr_area, 90,
                                          lv_color_hex(0x000000),
                                          lv_color_hex(0xFFFFFF));
    lv_qrcode_update(qr_wifi, wifi_qr_str, strlen(wifi_qr_str));
    lv_obj_set_pos(qr_wifi, 15, 14);

    /* 網頁 QR：掃描後開啟設定頁 */
    const char *url = wifi_manager_get_provisioning_url();
    lv_obj_t *qr_url = lv_qrcode_create(s_qr_area, 90,
                                          lv_color_hex(0x000000),
                                          lv_color_hex(0xFFFFFF));
    lv_qrcode_update(qr_url, url, strlen(url));
    lv_obj_set_pos(qr_url, 170, 14);
#else
    lv_obj_t *qr_fallback = lv_label_create(s_qr_area);
    lv_obj_set_pos(qr_fallback, 15, 14);
    lv_label_set_text(qr_fallback, "QR disabled\nenter URL manually");
    lv_obj_set_style_text_color(qr_fallback, lv_color_hex(0xFFCC66), 0);
    lv_obj_set_style_text_font(qr_fallback, &lv_font_montserrat_10, 0);
#endif

    /* AP 名稱 */
    lv_obj_t *ap_name_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(ap_name_lbl, 15, 106);
    lv_label_set_text(ap_name_lbl, wifi_manager_get_provisioning_ap_ssid());
    lv_obj_set_style_text_color(ap_name_lbl, lv_color_hex(0xDDE8F2), 0);
    lv_obj_set_style_text_font(ap_name_lbl, &lv_font_montserrat_10, 0);

    /* AP 密碼 */
    lv_obj_t *pw_info_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(pw_info_lbl, 15, 120);
    char pw_buf[64];
    snprintf(pw_buf, sizeof(pw_buf), "Pwd: %s", wifi_manager_get_provisioning_ap_password());
    lv_label_set_text(pw_info_lbl, pw_buf);
    lv_obj_set_style_text_color(pw_info_lbl, lv_color_hex(0xDDE8F2), 0);
    lv_obj_set_style_text_font(pw_info_lbl, &lv_font_montserrat_10, 0);

    /* Portal URL */
    lv_obj_t *url_info_lbl = lv_label_create(s_qr_area);
    lv_obj_set_pos(url_info_lbl, 170, 106);
    lv_label_set_text(url_info_lbl, wifi_manager_get_provisioning_url());
    lv_obj_set_style_text_color(url_info_lbl, lv_color_hex(0xDDE8F2), 0);
    lv_obj_set_style_text_font(url_info_lbl, &lv_font_montserrat_10, 0);

    /* Start / Stop Portal 按鈕 */
    s_btn_portal = lv_btn_create(s_screen);
    lv_obj_set_size(s_btn_portal, 110, 28);
    lv_obj_set_pos(s_btn_portal, (LCD_WIDTH - 110) / 2, 206);
    lv_obj_set_style_bg_color(s_btn_portal, lv_color_hex(0x8E44AD), 0);
    lv_obj_add_event_cb(s_btn_portal, btn_toggle_portal_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_portal = lv_label_create(s_btn_portal);
    lv_label_set_text(lbl_portal, "Start Portal");
    lv_obj_center(lbl_portal);

    /* 根據目前 Portal 狀態初始化 UI */
    update_portal_ui(wifi_manager_is_provisioning_portal_active());

    return s_screen;
}
