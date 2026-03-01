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
static lv_obj_t *s_batt_lbl   = NULL;
static lv_timer_t *s_update_timer = NULL;
static screen_id_t s_page = SCREEN_DASHBOARD;
static int s_wifi_state = 0;
static char s_wifi_ip[24] = {0};

static void get_time_hhmm(char *out, size_t out_sz)
{
    rtc_time_t rt;
    if (rtc_bm8563_get_time(&rt) == ESP_OK &&
        rt.hours < 24 && rt.minutes < 60) {
        snprintf(out, out_sz, "%02d:%02d", rt.hours, rt.minutes);
        return;
    }

    time_t now = time(NULL);
    if (now > 0) {
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        snprintf(out, out_sz, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        return;
    }

    snprintf(out, out_sz, "--:--");
}

static void update_time_label(lv_obj_t *label)
{
    if (!label) return;
    char buf[12];
    get_time_hhmm(buf, sizeof(buf));
    lv_label_set_text(label, buf);
}

static void refresh_page_message(void)
{
    if (!s_msg_lbl) return;

    char buf[128];
    if (s_page == SCREEN_WIFI) {
        if (s_wifi_state == 2 /* CONNECTED */) {
            snprintf(buf, sizeof(buf), "WiFi Setup - Connected (%s)",
                     (s_wifi_ip[0] != '\0') ? s_wifi_ip : "OK");
        } else if (s_wifi_state == 1 /* CONNECTING */) {
            snprintf(buf, sizeof(buf), "WiFi Setup - Connecting...");
        } else {
            snprintf(buf, sizeof(buf), "WiFi Setup - Offline");
        }
    } else if (s_page == SCREEN_SCHEDULE) {
        snprintf(buf, sizeof(buf),
                 "Schedule - Configure quote interval, AI interval and deep sleep");
    } else {
        if (s_wifi_state == 2 /* CONNECTED */) {
            snprintf(buf, sizeof(buf), "Dashboard - TWSE monitor (%s)",
                     (s_wifi_ip[0] != '\0') ? s_wifi_ip : "WiFi OK");
        } else if (s_wifi_state == 1 /* CONNECTING */) {
            snprintf(buf, sizeof(buf), "Dashboard - TWSE monitor (WiFi Connecting...)");
        } else {
            snprintf(buf, sizeof(buf), "Dashboard - TWSE monitor (WiFi Offline)");
        }
    }
    lv_label_set_text(s_msg_lbl, buf);
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
}

void status_bar_set_visible(bool visible)
{
    if (!s_bar) return;
    if (visible) lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_align(s_time_lbl, LV_ALIGN_LEFT_MID, 4, 0);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_opa(s_time_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_move_foreground(s_time_lbl);

    /* 中央頁面訊息（左右滾動） */
    s_msg_lbl = lv_label_create(bar);
    lv_obj_set_pos(s_msg_lbl, 56, 2);
    lv_obj_set_size(s_msg_lbl, LCD_WIDTH - 112, 16);
    lv_label_set_long_mode(s_msg_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_msg_lbl, "Dashboard");
    lv_obj_set_style_text_align(s_msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_msg_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_font(s_msg_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_anim_speed(s_msg_lbl, 30, 0);

    /* 電量 */
    s_batt_lbl = lv_label_create(bar);
    lv_obj_align(s_batt_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_label_set_text(s_batt_lbl, "BAT --%");
    lv_obj_set_style_text_color(s_batt_lbl, BAR_TEXT_COLOR, 0);
    lv_obj_set_style_text_font(s_batt_lbl, &lv_font_montserrat_10, 0);

    refresh_page_message();

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
}

void status_bar_set_page(screen_id_t page)
{
    s_page = page;
    refresh_page_message();
}
