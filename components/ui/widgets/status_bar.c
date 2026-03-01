#include "ui_manager.h"
#include "axp192.h"
#include "rtc_bm8563.h"
#include "app_config.h"
#include "ui_compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BAR_TEXT_COLOR lv_color_hex(0xDDE8F2)

/* 狀態列共用（所有頁面皆可見）*/
static lv_obj_t *s_bar        = NULL;
static lv_obj_t *s_time_lbl   = NULL;
static lv_obj_t *s_msg_lbl    = NULL;
static lv_obj_t *s_wifi_lbl   = NULL;
static lv_obj_t *s_batt_lbl   = NULL;
static lv_timer_t *s_update_timer = NULL;
static screen_id_t s_page = SCREEN_DASHBOARD;
static int s_wifi_state = 0;
static char s_wifi_ip[24] = {0};
static const int EDGE_PADDING = 4;
static const int ITEM_GAP = 6;

static void get_time_ampm(char *out, size_t out_sz)
{
    int hour24 = -1;
    int minute = -1;

    rtc_time_t rt;
    if (rtc_bm8563_get_time(&rt) == ESP_OK &&
        rt.hours < 24 && rt.minutes < 60) {
        hour24 = rt.hours;
        minute = rt.minutes;
    }

    if (hour24 < 0) {
        time_t now = time(NULL);
        if (now > 0) {
            struct tm tm_info;
            localtime_r(&now, &tm_info);
            hour24 = tm_info.tm_hour;
            minute = tm_info.tm_min;
        }
    }

    if (hour24 >= 0 && minute >= 0) {
        uint8_t hour12 = (uint8_t)(hour24 % 12);
        if (hour12 == 0) hour12 = 12;
        uint8_t minute_u8 = (uint8_t)minute;
        const char *ampm = (hour24 >= 12) ? "PM" : "AM";
        snprintf(out, out_sz, "%02u:%02u %s", hour12, minute_u8, ampm);
        return;
    }

    snprintf(out, out_sz, "--:--");
}

static void update_time_label(lv_obj_t *label)
{
    if (!label) return;
    char buf[20];
    get_time_ampm(buf, sizeof(buf));
    lv_label_set_text(label, buf);
}

static void refresh_page_message(void)
{
    if (!s_msg_lbl) return;

    char buf[128];
    const char *page_name = "Dashboard";
    if (s_page == SCREEN_LOG) {
        page_name = "Log";
    } else if (s_page == SCREEN_INFO) {
        page_name = "Info";
    }

    if (s_page == SCREEN_PORTAL) {
        if (s_wifi_state == 2 /* CONNECTED */) {
            snprintf(buf, sizeof(buf), "Portal Setup - Connected (%s)",
                     (s_wifi_ip[0] != '\0') ? s_wifi_ip : "OK");
        } else if (s_wifi_state == 1 /* CONNECTING */) {
            snprintf(buf, sizeof(buf), "Portal Setup - Connecting...");
        } else {
            snprintf(buf, sizeof(buf), "Portal Setup - Offline");
        }
    } else {
        if (s_wifi_state == 2 /* CONNECTED */) {
            snprintf(buf, sizeof(buf), "%s - TWSE monitor (%s)",
                     page_name, (s_wifi_ip[0] != '\0') ? s_wifi_ip : "WiFi OK");
        } else if (s_wifi_state == 1 /* CONNECTING */) {
            snprintf(buf, sizeof(buf), "%s - TWSE monitor (WiFi Connecting...)", page_name);
        } else {
            snprintf(buf, sizeof(buf), "%s - TWSE monitor (WiFi Offline)", page_name);
        }
    }
    lv_label_set_text(s_msg_lbl, buf);
}

static void refresh_wifi_icon(void)
{
    if (!s_wifi_lbl) return;

    if (s_wifi_state == 2 /* CONNECTED */) {
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0x4CAF50), 0);
    } else if (s_wifi_state == 1 /* CONNECTING */) {
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xFFB300), 0);
    } else {
        /* Keep the same glyph width for all states to avoid layout jitter. */
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xB71C1C), 0);
    }
}

static void layout_topbar_items(void)
{
    if (!s_bar || !s_time_lbl || !s_msg_lbl || !s_wifi_lbl || !s_batt_lbl) return;

    lv_obj_align(s_time_lbl, LV_ALIGN_LEFT_MID, EDGE_PADDING, 0);
    lv_obj_align(s_batt_lbl, LV_ALIGN_RIGHT_MID, -EDGE_PADDING, 0);
    lv_obj_align_to(s_wifi_lbl, s_batt_lbl, LV_ALIGN_OUT_LEFT_MID, -ITEM_GAP, 0);

    lv_obj_update_layout(s_bar);

    int msg_x = lv_obj_get_x(s_time_lbl) + lv_obj_get_width(s_time_lbl) + ITEM_GAP;
    int msg_right = lv_obj_get_x(s_wifi_lbl) - ITEM_GAP;
    int msg_w = msg_right - msg_x;
    if (msg_w < 60) msg_w = 60;

    lv_obj_set_pos(s_msg_lbl, msg_x, 2);
    lv_obj_set_size(s_msg_lbl, msg_w, 16);
}

static void status_update_cb(lv_timer_t *t)
{
    (void)t;

    /* 時間 */
    update_time_label(s_time_lbl);

    /* 電量 */
    uint8_t pct = axp192_get_battery_percent();
    bool charging = axp192_is_charging();
    if (s_batt_lbl) {
        char batt[12];
        snprintf(batt, sizeof(batt), charging ? "~%d%%" : "%d%%", pct);
        lv_label_set_text(s_batt_lbl, batt);
        lv_obj_set_style_text_color(s_batt_lbl, BAR_TEXT_COLOR, 0);
    }
    refresh_wifi_icon();
    layout_topbar_items();
}

void status_bar_create_on(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    s_bar = bar;
    lv_obj_set_size(bar, LCD_WIDTH, 20);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0F3460), 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 時間 */
    s_time_lbl = lv_label_create(bar);
    lv_obj_align(s_time_lbl, LV_ALIGN_LEFT_MID, EDGE_PADDING, 0);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_opa(s_time_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_move_foreground(s_time_lbl);

    /* 中央頁面訊息（左右滾動） */
    s_msg_lbl = lv_label_create(bar);
    lv_obj_set_pos(s_msg_lbl, 82, 2);
    lv_obj_set_size(s_msg_lbl, LCD_WIDTH - 152, 16);
    lv_label_set_long_mode(s_msg_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_msg_lbl, "Dashboard");
    lv_obj_set_style_text_align(s_msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_msg_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_font(s_msg_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_anim_speed(s_msg_lbl, 30, 0);

    /* 電量 */
    s_wifi_lbl = lv_label_create(bar);
    lv_obj_align(s_wifi_lbl, LV_ALIGN_RIGHT_MID, -48, 0);
    lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xB71C1C), 0);
    lv_obj_set_style_text_font(s_wifi_lbl, &lv_font_montserrat_14, 0);

    s_batt_lbl = lv_label_create(bar);
    lv_obj_align(s_batt_lbl, LV_ALIGN_RIGHT_MID, -EDGE_PADDING, 0);
    lv_label_set_text(s_batt_lbl, "BAT --%");
    lv_obj_set_style_text_color(s_batt_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_font(s_batt_lbl, &lv_font_montserrat_10, 0);

    refresh_page_message();
    refresh_wifi_icon();
    layout_topbar_items();

    /* 定時更新：每 10 秒 */
    if (!s_update_timer) {
        s_update_timer = lv_timer_create(status_update_cb, 10000, NULL);
        status_update_cb(NULL);  /* 立即更新一次 */
    }
}

void status_bar_update_wifi(int state, const char *ip)
{
    s_wifi_state = state;
    if (ip && ip[0] != '\0') {
        strncpy(s_wifi_ip, ip, sizeof(s_wifi_ip) - 1);
        s_wifi_ip[sizeof(s_wifi_ip) - 1] = '\0';
    } else {
        s_wifi_ip[0] = '\0';
    }
    refresh_page_message();
    refresh_wifi_icon();
    layout_topbar_items();
}

void status_bar_set_page(screen_id_t page)
{
    s_page = page;
    refresh_page_message();
}
