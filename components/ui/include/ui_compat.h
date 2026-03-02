#pragma once

#include "lvgl.h"
#include "ui_font.h"

/* Fallback fonts for configurations that only enable a subset of Montserrat sizes. */
#ifndef LV_FONT_MONTSERRAT_10
#define LV_FONT_MONTSERRAT_10 0
#endif
#ifndef LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 0
#endif
#ifndef LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 0
#endif

#define UI_FONT_TEXT_DEFAULT lv_font_noto_tc_14

#if !LV_FONT_MONTSERRAT_10
#define lv_font_montserrat_10 UI_FONT_TEXT_DEFAULT
#endif
#if !LV_FONT_MONTSERRAT_16
#define lv_font_montserrat_16 UI_FONT_TEXT_DEFAULT
#endif
#if !LV_FONT_MONTSERRAT_20
#define lv_font_montserrat_20 UI_FONT_TEXT_DEFAULT
#endif

/* API rename across LVGL versions. */
#ifndef lv_list_clean
#define lv_list_clean(list_obj) lv_obj_clean(list_obj)
#endif

/* Animation enum rename across LVGL versions. */
#ifndef LV_SCR_LOAD_ANIM_SLIDE_LEFT
#ifdef LV_SCR_LOAD_ANIM_MOVE_LEFT
#define LV_SCR_LOAD_ANIM_SLIDE_LEFT LV_SCR_LOAD_ANIM_MOVE_LEFT
#else
#define LV_SCR_LOAD_ANIM_SLIDE_LEFT LV_SCR_LOAD_ANIM_NONE
#endif
#endif
