#include "loading_spinner.h"
#include "app_config.h"

static lv_obj_t *s_overlay = NULL;

void loading_spinner_create(lv_obj_t *parent)
{
    s_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_80, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 180, 56);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_90, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xD0D0D0), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text(label, "Loading...");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_noto_tc_16, 0);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void loading_spinner_set_visible(bool visible)
{
    if (!s_overlay) return;
    if (visible) lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}
