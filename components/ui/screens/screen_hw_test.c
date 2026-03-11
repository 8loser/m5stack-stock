#include "screen_hw_test.h"
#include "alert_feedback.h"
#include "app_config.h"
#include "ui_compat.h"
#include <stdint.h>

#define COLOR_BG           lv_color_hex(0x121826)
#define COLOR_VIB_BTN_UP   lv_color_hex(0x1565C0)
#define COLOR_VIB_BTN_DOWN lv_color_hex(0x0D47A1)

static const lv_font_t *hw_test_text_font(void)
{
    return &lv_font_noto_tc_14;
}

static lv_obj_t *create_test_button(lv_obj_t *parent, int x, int y, int w, int h,
                                    lv_color_t bg_color, const char *text,
                                    lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x455A64), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, hw_test_text_font(), 0);
    lv_obj_center(label);

    return btn;
}

static void on_alert_up_clicked(lv_event_t *e)
{
    (void)e;
    alert_feedback_play(ALERT_FEEDBACK_UP);
}

static void on_alert_down_clicked(lv_event_t *e)
{
    (void)e;
    alert_feedback_play(ALERT_FEEDBACK_DOWN);
}

lv_obj_t *screen_hw_test_create(void)
{
    const int content_top = UI_CONTENT_TOP_Y + 72;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    create_test_button(screen, 12, content_top + 0, 298, 42, COLOR_VIB_BTN_UP, "上漲",
                       on_alert_up_clicked);
    create_test_button(screen, 12, content_top + 56, 298, 42, COLOR_VIB_BTN_DOWN, "下跌",
                       on_alert_down_clicked);

    return screen;
}
