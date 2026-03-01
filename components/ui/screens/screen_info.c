#include "app_config.h"
#include "ui_compat.h"

#define COLOR_BG lv_color_hex(0x122113)

lv_obj_t *screen_info_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Info");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Use left/right buttons\nto navigate pages");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xB3CCB6), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 8);

    return screen;
}
