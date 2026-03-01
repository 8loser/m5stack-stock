#include "ui_manager.h"
#include "scheduler.h"
#include "sleep_manager.h"
#include "storage.h"
#include "rtc_bm8563.h"
#include "app_config.h"
#include "ui_compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void clear_save_label_cb(lv_timer_t *t)
{
    lv_label_set_text((lv_obj_t *)t->user_data, "");
    lv_timer_del(t);
}

static lv_obj_t *s_screen          = NULL;
static lv_obj_t *s_quote_roller    = NULL;
static lv_obj_t *s_ai_roller       = NULL;
static lv_obj_t *s_market_sw       = NULL;
static lv_obj_t *s_sleep_sw        = NULL;
static lv_obj_t *s_save_lbl        = NULL;
static lv_obj_t *s_time_lbl        = NULL;
static lv_timer_t *s_time_timer    = NULL;

/* 報價更新間隔選項（秒）*/
static const uint16_t QUOTE_INTERVALS[]  = {30, 60, 300, 600};
static const char    *QUOTE_NAMES        = "30s\n1min\n5min\n10min";

/* AI 分析間隔選項（分鐘）—— 使用者可調整 */
static const uint16_t AI_INTERVALS[]  = {5, 10, 15, 30, 60, 120};
static const char    *AI_NAMES        = "5min\n10min\n15min\n30min\n1hr\n2hr";

static void do_save(void)
{
    schedule_config_t cfg;

    uint16_t qi = (uint16_t)lv_roller_get_selected(s_quote_roller);
    cfg.quote_interval_s = QUOTE_INTERVALS[qi];

    uint16_t ai = (uint16_t)lv_roller_get_selected(s_ai_roller);
    cfg.ai_interval_min = AI_INTERVALS[ai];

    cfg.market_only = lv_obj_has_state(s_market_sw, LV_STATE_CHECKED);

    bool sleep_en = lv_obj_has_state(s_sleep_sw, LV_STATE_CHECKED);
    sleep_manager_set_enabled(sleep_en);

    scheduler_apply_config(&cfg);

    lv_label_set_text(s_save_lbl, "Saved!");
    lv_timer_create(clear_save_label_cb, 2000, s_save_lbl);
}

static void btn_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    do_save();
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

lv_obj_t *screen_schedule_create(void)
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
    lv_label_set_text(title, "Schedule");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    if (!s_time_timer) {
        s_time_timer = lv_timer_create(time_update_cb, 10000, NULL);
    }
    update_topbar_time();

    /* ---- 報價更新間隔 ---- */
    lv_obj_t *ql = lv_label_create(s_screen);
    lv_obj_set_pos(ql, 8, 28);
    lv_label_set_text(ql, "Quote:");
    lv_obj_set_style_text_color(ql, lv_color_hex(0xCCCCCC), 0);

    s_quote_roller = lv_roller_create(s_screen);
    lv_roller_set_options(s_quote_roller, QUOTE_NAMES, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_quote_roller, 2);
    lv_obj_set_size(s_quote_roller, 100, 60);
    lv_obj_set_pos(s_quote_roller, 120, 22);

    /* 讀取當前設定 */
    schedule_config_t cur;
    scheduler_get_config(&cur);
    for (int i = 0; i < 4; i++) {
        if (QUOTE_INTERVALS[i] == cur.quote_interval_s) {
            lv_roller_set_selected(s_quote_roller, i, LV_ANIM_OFF);
            break;
        }
    }

    /* ---- AI 分析間隔（使用者可在機器上調整）---- */
    lv_obj_t *al = lv_label_create(s_screen);
    lv_obj_set_pos(al, 8, 92);
    lv_label_set_text(al, "AI Interval:");
    lv_obj_set_style_text_color(al, lv_color_hex(0xCCCCCC), 0);

    /* 副標題說明 */
    lv_obj_t *al_hint = lv_label_create(s_screen);
    lv_obj_set_pos(al_hint, 8, 106);
    lv_label_set_text(al_hint, "(adjustable on device)");
    lv_obj_set_style_text_color(al_hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(al_hint, &lv_font_montserrat_10, 0);

    s_ai_roller = lv_roller_create(s_screen);
    lv_roller_set_options(s_ai_roller, AI_NAMES, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_ai_roller, 2);
    lv_obj_set_size(s_ai_roller, 100, 60);
    lv_obj_set_pos(s_ai_roller, 200, 88);

    for (int i = 0; i < 6; i++) {
        if (AI_INTERVALS[i] == cur.ai_interval_min) {
            lv_roller_set_selected(s_ai_roller, i, LV_ANIM_OFF);
            break;
        }
    }

    /* ---- 市場時段限制開關 ---- */
    lv_obj_t *ml = lv_label_create(s_screen);
    lv_obj_set_pos(ml, 8, 156);
    lv_label_set_text(ml, "Market hours only");
    lv_obj_set_style_text_color(ml, lv_color_hex(0xCCCCCC), 0);

    s_market_sw = lv_switch_create(s_screen);
    lv_obj_set_pos(s_market_sw, 220, 152);
    if (cur.market_only) lv_obj_add_state(s_market_sw, LV_STATE_CHECKED);

    /* ---- DeepSleep 開關 ---- */
    lv_obj_t *sl = lv_label_create(s_screen);
    lv_obj_set_pos(sl, 8, 180);
    lv_label_set_text(sl, "Deep sleep");
    lv_obj_set_style_text_color(sl, lv_color_hex(0xCCCCCC), 0);

    s_sleep_sw = lv_switch_create(s_screen);
    lv_obj_set_pos(s_sleep_sw, 220, 176);
    if (sleep_manager_is_enabled()) lv_obj_add_state(s_sleep_sw, LV_STATE_CHECKED);

    /* ---- 儲存按鈕 ---- */
    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 90, 28);
    lv_obj_set_pos(btn_save, 115, 206);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x27AE60), 0);
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_center(lbl_save);

    s_save_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_save_lbl, 215, 212);
    lv_label_set_text(s_save_lbl, "");
    lv_obj_set_style_text_color(s_save_lbl, lv_color_hex(0x44BB44), 0);

    return s_screen;
}

void screen_schedule_on_btn(uint8_t btn)
{
    if (btn == 1) do_save();
}
