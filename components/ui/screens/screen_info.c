#include "screen_info.h"
#include "app_config.h"
#include "ui_compat.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define COLOR_BG       lv_color_hex(0x122113)
#define COLOR_SECTION  lv_color_hex(0x4FC3F7)
#define COLOR_BAR_BG   lv_color_hex(0x2D3E2F)
#define COLOR_BAR_FILL lv_color_hex(0x6CD46C)

static lv_obj_t *s_heap_lbl = NULL;
static lv_obj_t *s_heap_bar = NULL;
static lv_obj_t *s_stack_lbl = NULL;
static lv_obj_t *s_stack_bar = NULL;
static lv_obj_t *s_hint_lbl = NULL;
static lv_timer_t *s_refresh_timer = NULL;

static const lv_font_t *info_font(void)
{
    return &lv_font_noto_tc_14;
}

static void style_usage_bar(lv_obj_t *bar)
{
    lv_obj_set_size(bar, 280, 16);
    lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COLOR_BAR_FILL, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
}

void screen_info_refresh(void)
{
    if (s_heap_lbl == NULL || s_heap_bar == NULL || s_stack_lbl == NULL || s_stack_bar == NULL) {
        return;
    }

    {
        size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t heap_used = (heap_total > heap_free) ? (heap_total - heap_free) : 0;
        uint32_t heap_percent = (heap_total == 0) ? 0 : (uint32_t)((heap_used * 100U) / heap_total);

        lv_bar_set_value(s_heap_bar, (int32_t)heap_percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_heap_lbl,
                              "Heap current: %lu / %lu KB (%lu%%)",
                              (unsigned long)(heap_used / 1024U),
                              (unsigned long)(heap_total / 1024U),
                              (unsigned long)heap_percent);
    }

    {
        UBaseType_t hwm_words = uxTaskGetStackHighWaterMark(NULL);
        UBaseType_t total_words = (UBaseType_t)STACK_LVGL;
        UBaseType_t peak_used_words;
        uint32_t peak_percent;

        if (hwm_words > total_words) {
            hwm_words = total_words;
        }
        peak_used_words = total_words - hwm_words;
        peak_percent = (total_words == 0) ? 0 : (uint32_t)((peak_used_words * 100U) / total_words);

        lv_bar_set_value(s_stack_bar, (int32_t)peak_percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_stack_lbl,
                              "Stack peak (LVGL): %lu / %lu words (%lu%%)",
                              (unsigned long)peak_used_words,
                              (unsigned long)total_words,
                              (unsigned long)peak_percent);
    }
}

static void info_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    screen_info_refresh();
}

lv_obj_t *screen_info_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *container;

    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    container = lv_obj_create(screen);
    lv_obj_set_size(container, LCD_WIDTH, 190);
    lv_obj_set_pos(container, 0, UI_CONTENT_TOP_Y + 8);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_style_pad_row(container, 10, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    s_heap_lbl = lv_label_create(container);
    lv_label_set_text(s_heap_lbl, "Heap current: --");
    lv_obj_set_width(s_heap_lbl, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_heap_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_heap_lbl, info_font(), 0);

    s_heap_bar = lv_bar_create(container);
    style_usage_bar(s_heap_bar);

    s_stack_lbl = lv_label_create(container);
    lv_label_set_text(s_stack_lbl, "Stack peak (LVGL): --");
    lv_obj_set_width(s_stack_lbl, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_stack_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_stack_lbl, info_font(), 0);

    s_stack_bar = lv_bar_create(container);
    style_usage_bar(s_stack_bar);

    s_hint_lbl = lv_label_create(container);
    lv_label_set_text(s_hint_lbl, "Stack 顯示為峰值使用量（HWM 推算）");
    lv_obj_set_width(s_hint_lbl, LCD_WIDTH - 16);
    lv_obj_set_style_text_color(s_hint_lbl, lv_color_hex(0xA9C6AA), 0);
    lv_obj_set_style_text_font(s_hint_lbl, info_font(), 0);

    if (s_refresh_timer == NULL) {
        s_refresh_timer = lv_timer_create(info_refresh_timer_cb, 1000, NULL);
    }

    screen_info_refresh();
    return screen;
}
