#include "ui_manager.h"
#include "scheduler.h"
#include "sleep_manager.h"
#include "storage.h"
#include "app_config.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

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

/* 報價更新間隔選項（秒）*/
static const uint16_t QUOTE_INTERVALS[]  = {30, 60, 300, 600};
static const char    *QUOTE_NAMES        = "30秒\n1分\n5分\n10分";

/* AI 分析間隔選項（分鐘）—— 使用者可調整 */
static const uint16_t AI_INTERVALS[]  = {5, 10, 15, 30, 60, 120};
static const char    *AI_NAMES        = "5分\n10分\n15分\n30分\n1小時\n2小時";

static void btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_DASHBOARD);
}

static void btn_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    schedule_config_t cfg;

    /* 報價間隔 */
    uint16_t qi = (uint16_t)lv_roller_get_selected(s_quote_roller);
    cfg.quote_interval_s = QUOTE_INTERVALS[qi];

    /* AI 分析間隔（使用者在機器上調整）*/
    uint16_t ai = (uint16_t)lv_roller_get_selected(s_ai_roller);
    cfg.ai_interval_min = AI_INTERVALS[ai];

    /* 市場時段限制 */
    cfg.market_only = lv_obj_has_state(s_market_sw, LV_STATE_CHECKED);

    /* DeepSleep */
    bool sleep_en = lv_obj_has_state(s_sleep_sw, LV_STATE_CHECKED);
    sleep_manager_set_enabled(sleep_en);

    /* 套用並儲存 */
    scheduler_apply_config(&cfg);

    lv_label_set_text(s_save_lbl, "已儲存!");
    lv_timer_create(clear_save_label_cb, 2000, s_save_lbl);
}

lv_obj_t *screen_schedule_create(void)
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
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< 返回");
    lv_obj_center(lbl_back);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);
    lv_label_set_text(title, "排程設定");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* ---- 報價更新間隔 ---- */
    lv_obj_t *ql = lv_label_create(s_screen);
    lv_obj_set_pos(ql, 8, 48);
    lv_label_set_text(ql, "報價更新:");
    lv_obj_set_style_text_color(ql, lv_color_hex(0xCCCCCC), 0);

    s_quote_roller = lv_roller_create(s_screen);
    lv_roller_set_options(s_quote_roller, QUOTE_NAMES, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_quote_roller, 2);
    lv_obj_set_size(s_quote_roller, 100, 60);
    lv_obj_set_pos(s_quote_roller, 120, 42);

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
    lv_obj_set_pos(al, 8, 112);
    lv_label_set_text(al, "AI分析間隔:");
    lv_obj_set_style_text_color(al, lv_color_hex(0xCCCCCC), 0);

    /* 副標題說明 */
    lv_obj_t *al_hint = lv_label_create(s_screen);
    lv_obj_set_pos(al_hint, 8, 126);
    lv_label_set_text(al_hint, "(可在裝置上直接調整)");
    lv_obj_set_style_text_color(al_hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(al_hint, &lv_font_montserrat_10, 0);

    s_ai_roller = lv_roller_create(s_screen);
    lv_roller_set_options(s_ai_roller, AI_NAMES, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_ai_roller, 2);
    lv_obj_set_size(s_ai_roller, 100, 60);
    lv_obj_set_pos(s_ai_roller, 200, 108);

    for (int i = 0; i < 6; i++) {
        if (AI_INTERVALS[i] == cur.ai_interval_min) {
            lv_roller_set_selected(s_ai_roller, i, LV_ANIM_OFF);
            break;
        }
    }

    /* ---- 市場時段限制開關 ---- */
    lv_obj_t *ml = lv_label_create(s_screen);
    lv_obj_set_pos(ml, 8, 176);
    lv_label_set_text(ml, "僅市場時段更新");
    lv_obj_set_style_text_color(ml, lv_color_hex(0xCCCCCC), 0);

    s_market_sw = lv_switch_create(s_screen);
    lv_obj_set_pos(s_market_sw, 220, 172);
    if (cur.market_only) lv_obj_add_state(s_market_sw, LV_STATE_CHECKED);

    /* ---- DeepSleep 開關 ---- */
    lv_obj_t *sl = lv_label_create(s_screen);
    lv_obj_set_pos(sl, 8, 200);
    lv_label_set_text(sl, "休市睡眠省電");
    lv_obj_set_style_text_color(sl, lv_color_hex(0xCCCCCC), 0);

    s_sleep_sw = lv_switch_create(s_screen);
    lv_obj_set_pos(s_sleep_sw, 220, 196);
    if (sleep_manager_is_enabled()) lv_obj_add_state(s_sleep_sw, LV_STATE_CHECKED);

    /* ---- 儲存按鈕 ---- */
    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 90, 28);
    lv_obj_set_pos(btn_save, 115, 210);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x27AE60), 0);
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "儲存設定");
    lv_obj_center(lbl_save);

    s_save_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_save_lbl, 215, 216);
    lv_label_set_text(s_save_lbl, "");
    lv_obj_set_style_text_color(s_save_lbl, lv_color_hex(0x44BB44), 0);

    return s_screen;
}
