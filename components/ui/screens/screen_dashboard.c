#include "ui_manager.h"
#include "app_config.h"
#include "twse_models.h"
#include "scheduler.h"
#include "ui_compat.h"
#include <stdio.h>
#include <string.h>

/* ====== 顏色定義 ====== */
#define COLOR_UP    lv_color_hex(0xFF4444)   /* 漲：紅 */
#define COLOR_DOWN  lv_color_hex(0x44BB44)   /* 跌：綠 */
#define COLOR_FLAT  lv_color_hex(0xFFFFFF)   /* 平：白 */
#define COLOR_BG    lv_color_hex(0x1A1A2E)   /* 深藍背景 */
#define COLOR_CARD  lv_color_hex(0x16213E)   /* 卡片背景 */

#define MAX_CARDS   5
static lv_obj_t *s_screen  = NULL;
static lv_obj_t *s_cards[MAX_CARDS] = {NULL};
static lv_obj_t *s_name_labels[MAX_CARDS]   = {NULL};
static lv_obj_t *s_price_labels[MAX_CARDS]  = {NULL};
static lv_obj_t *s_change_labels[MAX_CARDS] = {NULL};
static lv_obj_t *s_update_label = NULL;

lv_obj_t *screen_dashboard_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 股票卡片列表 ---- */
    for (int i = 0; i < MAX_CARDS; i++) {
        s_cards[i] = lv_obj_create(s_screen);
        lv_obj_set_size(s_cards[i], LCD_WIDTH - 8, 32);
        lv_obj_set_pos(s_cards[i], 4, 24 + i * 36);
        lv_obj_set_style_bg_color(s_cards[i], COLOR_CARD, 0);
        lv_obj_set_style_radius(s_cards[i], 4, 0);
        lv_obj_clear_flag(s_cards[i], LV_OBJ_FLAG_SCROLLABLE);

        /* 股票名稱（左）*/
        s_name_labels[i] = lv_label_create(s_cards[i]);
        lv_obj_set_pos(s_name_labels[i], 4, 4);
        lv_label_set_text(s_name_labels[i], "---");
        lv_obj_set_style_text_color(s_name_labels[i], lv_color_white(), 0);

        /* 現價（中）*/
        s_price_labels[i] = lv_label_create(s_cards[i]);
        lv_obj_align(s_price_labels[i], LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(s_price_labels[i], "---.--");
        lv_obj_set_style_text_color(s_price_labels[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_price_labels[i], &lv_font_montserrat_16, 0);

        /* 漲跌幅（右）*/
        s_change_labels[i] = lv_label_create(s_cards[i]);
        lv_obj_align(s_change_labels[i], LV_ALIGN_RIGHT_MID, -4, 0);
        lv_label_set_text(s_change_labels[i], "±0.00%");
        lv_obj_set_style_text_color(s_change_labels[i], lv_color_white(), 0);
    }

    /* 最後更新時間 */
    s_update_label = lv_label_create(s_screen);
    lv_obj_set_pos(s_update_label, 4, 206);
    lv_label_set_text(s_update_label, "Updated: --:--:--");
    lv_obj_set_style_text_color(s_update_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_update_label, &lv_font_montserrat_10, 0);

    return s_screen;
}

void screen_dashboard_on_btn(uint8_t btn)
{
    if (btn == 1) scheduler_trigger_quote_now();
}

void screen_dashboard_update(const stock_quote_t *q)
{
    /* 找對應的卡片 index（依目前顯示順序）*/
    static char symbols_order[MAX_CARDS][8] = {0};
    static int  card_count = 0;

    int idx = -1;
    for (int i = 0; i < card_count; i++) {
        if (strcmp(symbols_order[i], q->symbol) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1 && card_count < MAX_CARDS) {
        idx = card_count;
        strncpy(symbols_order[card_count++], q->symbol, 7);
    }
    if (idx < 0 || idx >= MAX_CARDS) return;

    /* 更新名稱 */
    char name_str[48];
    snprintf(name_str, sizeof(name_str), "%s\n%s", q->symbol, q->name);
    lv_label_set_text(s_name_labels[idx], name_str);

    /* 更新價格 */
    char price_str[16];
    if (q->is_market_closed) {
        snprintf(price_str, sizeof(price_str), "%.2f\nclosed", q->yesterday_close);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_hex(0x888888), 0);
    } else {
        snprintf(price_str, sizeof(price_str), "%.2f", q->current_price);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
    }
    lv_label_set_text(s_price_labels[idx], price_str);

    /* 更新漲跌幅 + 顏色 */
    char change_str[16];
    snprintf(change_str, sizeof(change_str), "%+.2f%%", q->change_percent);
    lv_label_set_text(s_change_labels[idx], change_str);

    lv_color_t color;
    if (q->change_percent > 0.01f)       color = COLOR_UP;
    else if (q->change_percent < -0.01f) color = COLOR_DOWN;
    else                                  color = COLOR_FLAT;
    lv_obj_set_style_text_color(s_change_labels[idx], color, 0);
    lv_obj_set_style_bg_color(s_cards[idx],
                               (q->change_percent > 0.01f)  ? lv_color_hex(0x2D0000) :
                               (q->change_percent < -0.01f) ? lv_color_hex(0x002D00) :
                               COLOR_CARD, 0);

    /* 更新時間 */
    char time_str[40];
    snprintf(time_str, sizeof(time_str), "Updated: %s", q->trade_time);
    lv_label_set_text(s_update_label, time_str);
}
