#include "ui_manager.h"
#include "ai_provider.h"
#include "scheduler.h"
#include "app_config.h"
#include "lvgl.h"
#include "audio.h"
#include "vibration.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_screen       = NULL;
static lv_obj_t *s_provider_lbl = NULL;
static lv_obj_t *s_signal_lbl   = NULL;
static lv_obj_t *s_conf_bar     = NULL;
static lv_obj_t *s_conf_lbl     = NULL;
static lv_obj_t *s_analysis_ta  = NULL;
static lv_obj_t *s_stock_lbl    = NULL;
static lv_obj_t *s_error_lbl    = NULL;

static const char *SIGNAL_TEXT[] = {"買入", "賣出", "觀望", "未知"};
static lv_color_t  SIGNAL_COLOR[] = {
    [AI_SIGNAL_BUY]     = {.full = 0},  /* 紅（初始化在 create）*/
    [AI_SIGNAL_SELL]    = {.full = 0},
    [AI_SIGNAL_HOLD]    = {.full = 0},
    [AI_SIGNAL_UNKNOWN] = {.full = 0},
};

static void btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        ui_manager_switch_screen(SCREEN_DASHBOARD);
}
static void btn_analyze_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_label_set_text(s_analysis_ta, "分析中...");
        scheduler_trigger_ai_now();
    }
}

lv_obj_t *screen_ai_analysis_create(void)
{
    SIGNAL_COLOR[AI_SIGNAL_BUY]     = lv_color_hex(0xFF4444);
    SIGNAL_COLOR[AI_SIGNAL_SELL]    = lv_color_hex(0x44BB44);
    SIGNAL_COLOR[AI_SIGNAL_HOLD]    = lv_color_hex(0xFFAA00);
    SIGNAL_COLOR[AI_SIGNAL_UNKNOWN] = lv_color_hex(0x888888);

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x1A1A2E), 0);

    /* 狀態列 */
    lv_obj_t *bar = lv_obj_create(s_screen);
    lv_obj_set_size(bar, LCD_WIDTH, 20);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0F3460), 0);

    /* 返回按鈕 */
    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 55, 24);
    lv_obj_set_pos(btn_back, 4, 22);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2C3E50), 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< 返回");
    lv_obj_center(lbl_back);

    /* 標題：Provider + 股票 */
    s_provider_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_provider_lbl, 65, 24);
    lv_label_set_text_fmt(s_provider_lbl, "AI: %s", ai_provider_get_name());
    lv_obj_set_style_text_color(s_provider_lbl, lv_color_hex(0xAAAAAA), 0);

    s_stock_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_stock_lbl, 180, 24);
    lv_label_set_text(s_stock_lbl, "---");
    lv_obj_set_style_text_color(s_stock_lbl, lv_color_white(), 0);

    /* 訊號 */
    s_signal_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_signal_lbl, 4, 50);
    lv_label_set_text(s_signal_lbl, "訊號: ---");
    lv_obj_set_style_text_color(s_signal_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_signal_lbl, &lv_font_montserrat_20, 0);

    /* 信心度 Bar */
    lv_obj_t *conf_cont = lv_obj_create(s_screen);
    lv_obj_set_size(conf_cont, LCD_WIDTH - 8, 20);
    lv_obj_set_pos(conf_cont, 4, 76);
    lv_obj_set_style_bg_color(conf_cont, lv_color_hex(0x0F3460), 0);
    lv_obj_clear_flag(conf_cont, LV_OBJ_FLAG_SCROLLABLE);

    s_conf_bar = lv_bar_create(conf_cont);
    lv_obj_set_size(s_conf_bar, LCD_WIDTH - 80, 12);
    lv_obj_set_pos(s_conf_bar, 60, 4);
    lv_bar_set_range(s_conf_bar, 0, 100);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_ON);

    lv_obj_t *conf_title = lv_label_create(conf_cont);
    lv_obj_set_pos(conf_title, 4, 2);
    lv_label_set_text(conf_title, "信心度:");
    lv_obj_set_style_text_color(conf_title, lv_color_hex(0xAAAAAA), 0);

    s_conf_lbl = lv_label_create(conf_cont);
    lv_obj_align(s_conf_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_label_set_text(s_conf_lbl, "0%");
    lv_obj_set_style_text_color(s_conf_lbl, lv_color_white(), 0);

    /* 分析文字（可捲動）*/
    s_analysis_ta = lv_label_create(s_screen);
    lv_obj_set_size(s_analysis_ta, LCD_WIDTH - 8, 110);
    lv_obj_set_pos(s_analysis_ta, 4, 102);
    lv_label_set_long_mode(s_analysis_ta, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_analysis_ta, "等待分析...");
    lv_obj_set_style_text_color(s_analysis_ta, lv_color_white(), 0);

    /* 立即分析按鈕 */
    lv_obj_t *btn_analyze = lv_btn_create(s_screen);
    lv_obj_set_size(btn_analyze, 100, 28);
    lv_obj_set_pos(btn_analyze, 110, 210);
    lv_obj_set_style_bg_color(btn_analyze, lv_color_hex(0x533483), 0);
    lv_obj_add_event_cb(btn_analyze, btn_analyze_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_analyze = lv_label_create(btn_analyze);
    lv_label_set_text(lbl_analyze, "立即分析");
    lv_obj_center(lbl_analyze);

    /* 錯誤訊息 */
    s_error_lbl = lv_label_create(s_screen);
    lv_obj_set_pos(s_error_lbl, 4, 216);
    lv_label_set_text(s_error_lbl, "");
    lv_obj_set_style_text_color(s_error_lbl, lv_color_hex(0xFF6666), 0);

    return s_screen;
}

void screen_ai_analysis_update(const ai_analysis_result_t *r)
{
    if (r->error_code != ESP_OK) {
        lv_label_set_text(s_error_lbl, "分析失敗，請重試");
        lv_label_set_text(s_analysis_ta, "");
        return;
    }

    lv_label_set_text(s_error_lbl, "");

    /* 更新 Provider 名稱 */
    lv_label_set_text_fmt(s_provider_lbl, "AI: %s", ai_provider_get_name());

    /* 訊號 */
    ai_signal_t sig = r->signal < 4 ? r->signal : AI_SIGNAL_UNKNOWN;
    lv_label_set_text_fmt(s_signal_lbl, "訊號: %s", SIGNAL_TEXT[sig]);
    lv_obj_set_style_text_color(s_signal_lbl, SIGNAL_COLOR[sig], 0);

    /* 信心度 */
    lv_bar_set_value(s_conf_bar, r->confidence, LV_ANIM_ON);
    lv_label_set_text_fmt(s_conf_lbl, "%d%%", r->confidence);

    /* 分析文字 */
    lv_label_set_text(s_analysis_ta, r->analysis);

    /* 音效 + 震動提示 */
    audio_notify();
    if (sig == AI_SIGNAL_BUY)       audio_alert_up();
    else if (sig == AI_SIGNAL_SELL) audio_alert_down();
}
