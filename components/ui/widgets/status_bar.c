#include "ui_manager.h"
#include "axp192.h"
#include "rtc_bm8563.h"
#include "app_config.h"
#include "lvgl.h"
#include <stdio.h>

/* 狀態列共用（所有頁面皆可見）*/
static lv_obj_t *s_time_lbl   = NULL;
static lv_obj_t *s_wifi_lbl   = NULL;
static lv_obj_t *s_batt_lbl   = NULL;
static lv_timer_t *s_update_timer = NULL;

static void status_update_cb(lv_timer_t *t)
{
    /* 時間 */
    rtc_time_t rt;
    if (rtc_bm8563_get_time(&rt) == ESP_OK) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d", rt.hours, rt.minutes);
        if (s_time_lbl) lv_label_set_text(s_time_lbl, buf);
    }

    /* 電量 */
    uint8_t pct = axp192_get_battery_percent();
    bool charging = axp192_is_charging();
    if (s_batt_lbl) {
        char batt[12];
        snprintf(batt, sizeof(batt), charging ? "~%d%%" : "%d%%", pct);
        lv_label_set_text(s_batt_lbl, batt);
        lv_color_t c = (pct > 30) ? lv_color_hex(0x44BB44) : lv_color_hex(0xFF4444);
        lv_obj_set_style_text_color(s_batt_lbl, c, 0);
    }
}

void status_bar_create_on(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LCD_WIDTH, 20);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0F3460), 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 時間 */
    s_time_lbl = lv_label_create(bar);
    lv_obj_set_pos(s_time_lbl, 4, 2);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_10, 0);

    /* WiFi 狀態 */
    s_wifi_lbl = lv_label_create(bar);
    lv_obj_align(s_wifi_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " --");
    lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_wifi_lbl, &lv_font_montserrat_10, 0);

    /* 電量 */
    s_batt_lbl = lv_label_create(bar);
    lv_obj_align(s_batt_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_label_set_text(s_batt_lbl, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(s_batt_lbl, lv_color_hex(0x44BB44), 0);
    lv_obj_set_style_text_font(s_batt_lbl, &lv_font_montserrat_10, 0);

    /* 定時更新：每 10 秒 */
    if (!s_update_timer) {
        s_update_timer = lv_timer_create(status_update_cb, 10000, NULL);
        status_update_cb(NULL);  /* 立即更新一次 */
    }
}

void status_bar_update_wifi(int state, const char *ip)
{
    if (!s_wifi_lbl) return;
    char buf[32];
    if (state == 2 /* CONNECTED */) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s", ip ? ip : "OK");
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0x44BB44), 0);
    } else if (state == 1 /* CONNECTING */) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " 連線中");
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xFFAA00), 0);
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " 未連線");
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xFF4444), 0);
    }
    lv_label_set_text(s_wifi_lbl, buf);
}
