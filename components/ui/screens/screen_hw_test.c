#include "audio.h"
#include "ui_compat.h"
#include "vibration.h"
#include <stdint.h>

#define COLOR_BG           lv_color_hex(0x121826)
#define COLOR_VIB_SECTION  lv_color_hex(0x4FC3F7)
#define COLOR_AUDIO_SECTION lv_color_hex(0xCE93D8)
#define COLOR_VIB_BTN      lv_color_hex(0x1565C0)
#define COLOR_AUDIO_BTN    lv_color_hex(0x4A148C)

static const lv_font_t *hw_test_text_font(void)
{
#if defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#elif defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#else
    return LV_FONT_DEFAULT;
#endif
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

static void on_alert_clicked(lv_event_t *e)
{
    (void)e;
    vibration_alert();
}

static void on_beep_clicked(lv_event_t *e)
{
    (void)e;
    audio_beep(880, 80);
    audio_beep(1047, 80);
    audio_beep(1319, 80);
    audio_beep(1568, 80);
}

lv_obj_t *screen_hw_test_create(void)
{
    const int content_top = 72;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *vibration_label = lv_label_create(screen);
    lv_label_set_text(vibration_label, "VIBRATION TEST");
    lv_obj_set_style_text_font(vibration_label, hw_test_text_font(), 0);
    lv_obj_set_style_text_color(vibration_label, COLOR_VIB_SECTION, 0);
    lv_obj_set_pos(vibration_label, 12, content_top + 0);

    create_test_button(screen, 12, content_top + 16, 298, 40, COLOR_VIB_BTN, "Vibrate",
                       on_alert_clicked);

    lv_obj_t *audio_label = lv_label_create(screen);
    lv_label_set_text(audio_label, "AUDIO TEST");
    lv_obj_set_style_text_font(audio_label, hw_test_text_font(), 0);
    lv_obj_set_style_text_color(audio_label, COLOR_AUDIO_SECTION, 0);
    lv_obj_set_pos(audio_label, 12, content_top + 60);

    create_test_button(screen, 12, content_top + 76, 298, 40, COLOR_AUDIO_BTN, "Beep",
                       on_beep_clicked);

    return screen;
}
