#include "lvgl.h"
#include "app_config.h"

static lv_obj_t *s_spinner = NULL;

void loading_spinner_create(lv_obj_t *parent)
{
    s_spinner = lv_spinner_create(parent, 1000, 60);
    lv_obj_set_size(s_spinner, 50, 50);
    lv_obj_align(s_spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
}

void loading_spinner_set_visible(bool visible)
{
    if (!s_spinner) return;
    if (visible) lv_obj_clear_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
}
