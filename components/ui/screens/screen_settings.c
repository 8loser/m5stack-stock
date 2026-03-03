#include "ui_manager.h"
#include "app_config.h"
#include "scheduler.h"
#include "storage.h"
#include "board.h"
#include "ui_compat.h"
#include "esp_log.h"
#include <stdint.h>

#define COLOR_BG      lv_color_hex(0x1A1A2E)
#define COLOR_TOGGLE_OFF lv_color_hex(0x37474F)
#define COLOR_TOGGLE_ON  lv_color_hex(0x1E88E5)

static const uint16_t INTERVAL_VALUES[] = {60, 300, 600};
static const char *INTERVAL_LABELS[] = {"1 分鐘", "5 分鐘", "10 分鐘"};
static const char *TAG = "screen_settings";

static lv_obj_t *s_interval_btns[3] = {0};
static lv_obj_t *s_interval_lbls[3] = {0};
static lv_obj_t *s_feedback_lbl = NULL;
static lv_obj_t *s_brightness_slider = NULL;
static lv_obj_t *s_brightness_lbl = NULL;
static uint16_t s_selected_interval_idx = 0;

static const lv_font_t *settings_font(void)
{
    return &lv_font_noto_tc_14;
}

static const lv_font_t *settings_title_font(void)
{
    return &lv_font_noto_tc_16;
}

static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

static void update_interval_toggle_styles(void)
{
    for (uint16_t i = 0; i < 3; i++) {
        bool selected = (i == s_selected_interval_idx);
        lv_obj_set_style_bg_color(s_interval_btns[i],
                                  selected ? COLOR_TOGGLE_ON : COLOR_TOGGLE_OFF, 0);
        lv_obj_set_style_text_color(s_interval_lbls[i], lv_color_white(), 0);
    }
}

static void persist_selected_interval(void)
{
    schedule_config_t cfg = {0};
    uint16_t idx = s_selected_interval_idx;
    esp_err_t apply_ret;
    esp_err_t save_ret;

    if (idx >= 3) {
        idx = 1;
    }

    scheduler_get_config(&cfg);
    cfg.quote_interval_s = INTERVAL_VALUES[idx];
    cfg.market_only = true;

    apply_ret = scheduler_apply_config(&cfg);
    save_ret = storage_schedule_save(&cfg);
    if (apply_ret == ESP_OK && save_ret == ESP_OK) {
        lv_label_set_text(s_feedback_lbl, "儲存成功");
        lv_obj_set_style_text_color(s_feedback_lbl, lv_color_hex(0x81C784), 0);
        ESP_LOGI(TAG, "saved: quote=%us market_only=1", cfg.quote_interval_s);
    } else {
        lv_label_set_text(s_feedback_lbl, "儲存失敗");
        lv_obj_set_style_text_color(s_feedback_lbl, lv_color_hex(0xEF5350), 0);
        ESP_LOGW(TAG, "save failed: apply=%s storage=%s",
                 esp_err_to_name(apply_ret), esp_err_to_name(save_ret));
    }
}

static void on_interval_clicked(lv_event_t *e)
{
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if (idx >= 3) {
        return;
    }
    s_selected_interval_idx = (uint16_t)idx;
    update_interval_toggle_styles();
    persist_selected_interval();
}

static void update_brightness_label(uint8_t level)
{
    if (s_brightness_lbl == NULL) return;
    lv_label_set_text_fmt(s_brightness_lbl, "Brightness: %u%%", (unsigned)level);
}

static uint8_t slider_value_to_u8(lv_obj_t *slider)
{
    int32_t value = lv_slider_get_value(slider);
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    return (uint8_t)value;
}

static void on_brightness_value_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t level = slider_value_to_u8(slider);
    esp_err_t ret = board_set_lcd_brightness(level);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "brightness apply failed: %s", esp_err_to_name(ret));
    }
    update_brightness_label(level);
}

static void on_brightness_released(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t level = slider_value_to_u8(slider);
    esp_err_t ret = storage_display_save(level);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "brightness save failed: %s", esp_err_to_name(ret));
    }
}

void screen_settings_load(void)
{
    schedule_config_t cfg = {0};
    uint8_t brightness = 80;
    uint16_t best_idx = 1;
    uint16_t best_diff = UINT16_MAX;
    uint16_t i;

    if (s_interval_btns[0] == NULL) {
        return;
    }

    scheduler_get_config(&cfg);

    for (i = 0; i < 3; i++) {
        uint16_t diff = abs_diff_u16(INTERVAL_VALUES[i], cfg.quote_interval_s);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    s_selected_interval_idx = best_idx;
    update_interval_toggle_styles();
    if (s_feedback_lbl != NULL) {
        lv_label_set_text(s_feedback_lbl, "");
        lv_obj_set_style_text_color(s_feedback_lbl, lv_color_hex(0xB0BEC5), 0);
    }

    if (storage_display_load(&brightness) != ESP_OK) {
        brightness = 80;
    }
    if (brightness > 100) brightness = 100;
    if (s_brightness_slider != NULL) {
        lv_slider_set_value(s_brightness_slider, brightness, LV_ANIM_OFF);
    }
    update_brightness_label(brightness);

    ESP_LOGI(TAG, "loaded: quote=%us selected_idx=%u",
             cfg.quote_interval_s, (unsigned)best_idx);
}

lv_obj_t *screen_settings_create(void)
{
    const lv_coord_t content_top = UI_CONTENT_TOP_Y + 40;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *interval_label;

    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    interval_label = lv_label_create(screen);
    lv_label_set_text(interval_label, "報價間隔");
    lv_obj_set_width(interval_label, LCD_WIDTH);
    lv_obj_set_style_text_align(interval_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(interval_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(interval_label, settings_title_font(), 0);
    lv_obj_set_pos(interval_label, 0, content_top);

    for (uint16_t i = 0; i < 3; i++) {
        s_interval_btns[i] = lv_btn_create(screen);
        lv_obj_set_size(s_interval_btns[i], 80, 32);
        lv_obj_set_pos(s_interval_btns[i], 20 + 100 * i, content_top + 30);
        lv_obj_set_style_bg_color(s_interval_btns[i], COLOR_TOGGLE_OFF, 0);
        lv_obj_set_style_border_width(s_interval_btns[i], 1, 0);
        lv_obj_set_style_border_color(s_interval_btns[i], lv_color_hex(0x78909C), 0);
        lv_obj_add_event_cb(s_interval_btns[i], on_interval_clicked, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        s_interval_lbls[i] = lv_label_create(s_interval_btns[i]);
        lv_label_set_text(s_interval_lbls[i], INTERVAL_LABELS[i]);
        lv_obj_set_style_text_font(s_interval_lbls[i], settings_font(), 0);
        lv_obj_center(s_interval_lbls[i]);
    }

    s_feedback_lbl = lv_label_create(screen);
    lv_label_set_text(s_feedback_lbl, "");
    lv_obj_set_width(s_feedback_lbl, 280);
    lv_obj_set_style_text_align(s_feedback_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_feedback_lbl, settings_font(), 0);
    lv_obj_set_style_text_color(s_feedback_lbl, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_pos(s_feedback_lbl, 20, content_top + 74);

    s_brightness_lbl = lv_label_create(screen);
    lv_label_set_text(s_brightness_lbl, "Brightness: 80%");
    lv_obj_set_style_text_color(s_brightness_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_brightness_lbl, settings_font(), 0);
    lv_obj_set_pos(s_brightness_lbl, 20, content_top + 106);

    s_brightness_slider = lv_slider_create(screen);
    lv_obj_set_size(s_brightness_slider, 280, 16);
    lv_obj_set_pos(s_brightness_slider, 20, content_top + 130);
    lv_slider_set_range(s_brightness_slider, 0, 100);
    lv_slider_set_value(s_brightness_slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_brightness_slider, on_brightness_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_brightness_slider, on_brightness_released, LV_EVENT_RELEASED, NULL);

    screen_settings_load();
    return screen;
}
