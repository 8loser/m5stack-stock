#include "ui_compat.h"
#include "twse_models.h"
#include <stdio.h>

/* 獨立股票卡片 widget（可在 Dashboard 外複用）*/
lv_obj_t *stock_card_create(lv_obj_t *parent, const stock_quote_t *q)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 148, 70);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 代號 */
    lv_obj_t *sym = lv_label_create(card);
    lv_obj_set_pos(sym, 4, 4);
    lv_label_set_text(sym, q->symbol);
    lv_obj_set_style_text_color(sym, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_10, 0);

    /* 名稱 */
    lv_obj_t *name = lv_label_create(card);
    lv_obj_set_pos(name, 4, 18);
    lv_label_set_text(name, q->name);
    lv_obj_set_style_text_color(name, lv_color_white(), 0);

    /* 現價 */
    lv_obj_t *price = lv_label_create(card);
    lv_obj_align(price, LV_ALIGN_CENTER, 0, 10);
    char price_str[16];
    snprintf(price_str, sizeof(price_str), "%.2f", q->current_price);
    lv_label_set_text(price, price_str);
    lv_obj_set_style_text_font(price, &lv_font_montserrat_20, 0);

    /* 漲跌幅 */
    lv_obj_t *chg = lv_label_create(card);
    lv_obj_align(chg, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    char chg_str[12];
    snprintf(chg_str, sizeof(chg_str), "%+.2f%%", q->change_percent);
    lv_label_set_text(chg, chg_str);
    lv_color_t c = (q->change_percent > 0.01f)  ? lv_color_hex(0xFF4444) :
                   (q->change_percent < -0.01f)  ? lv_color_hex(0x44BB44) :
                   lv_color_white();
    lv_obj_set_style_text_color(chg, c, 0);

    return card;
}
