#include "screen_boot.h"
#include "app_config.h"
#include "ui_compat.h"

static lv_obj_t *s_progress_label = NULL;

lv_obj_t *screen_boot_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *title = NULL;
    lv_obj_t *subtitle = NULL;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(screen);
    lv_label_set_text(title, "M5Stack Stock");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -18);
    lv_obj_set_style_text_color(title, lv_color_hex(0xDCEBFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Booting firmware...");
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9BB6D8), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_noto_tc_14, 0);

    s_progress_label = lv_label_create(screen);
    lv_label_set_text(s_progress_label, "Initialize...");
    lv_obj_align(s_progress_label, LV_ALIGN_CENTER, 0, 36);
    lv_obj_set_width(s_progress_label, LCD_WIDTH - 24);
    lv_obj_set_style_text_align(s_progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_progress_label, lv_color_hex(0x84D8C9), 0);
    lv_obj_set_style_text_font(s_progress_label, &lv_font_noto_tc_14, 0);

    return screen;
}

void screen_boot_set_progress(const char *text)
{
    if (s_progress_label == NULL) {
        return;
    }

    if (text == NULL || text[0] == '\0') {
        lv_label_set_text(s_progress_label, "Initialize...");
        return;
    }

    lv_label_set_text(s_progress_label, text);
}
