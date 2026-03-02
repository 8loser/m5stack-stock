#include "ui_manager.h"
#include "app_config.h"
#include "scheduler.h"
#include "twse_models.h"
#include "ui_compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ====== 顏色定義 ====== */
#define COLOR_UP    lv_color_hex(0xFF4444)   /* 漲：紅 */
#define COLOR_DOWN  lv_color_hex(0x44BB44)   /* 跌：綠 */
#define COLOR_FLAT  lv_color_hex(0xFFFFFF)   /* 平：白 */
#define COLOR_BG    lv_color_hex(0x1A1A2E)   /* 深藍背景 */
#define COLOR_CARD  lv_color_hex(0x16213E)   /* 卡片背景 */
#define DASHBOARD_VISIBLE_ROWS 5

static lv_obj_t *s_screen  = NULL;
static lv_obj_t *s_card_list = NULL;
static lv_obj_t *s_cards[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_name_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_price_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_change_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_empty_label = NULL;
static lv_obj_t *s_update_label = NULL;
static lv_obj_t *s_next_label = NULL;
static lv_timer_t *s_page_flip_timer = NULL;

/* 快取最新報價，供非 active 狀態下累積，切回 dashboard 時重新套用 */
static char          s_symbols_order[MAX_STOCK_COUNT][8] = {0};
static int           s_card_count = 0;
static int           s_cur_page = 0;
static stock_quote_t s_cached_quotes[MAX_STOCK_COUNT];
static bool          s_cached_valid[MAX_STOCK_COUNT] = {false};

static void apply_card_widgets(int idx, const stock_quote_t *q);

static void ensure_card_widgets(int idx)
{
    if (idx < 0 || idx >= DASHBOARD_VISIBLE_ROWS) {
        return;
    }

    if (!s_name_labels[idx]) {
        s_name_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_set_pos(s_name_labels[idx], 4, 4);
        lv_label_set_text(s_name_labels[idx], "---");
        lv_obj_set_style_text_color(s_name_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_name_labels[idx], &lv_font_noto_tc_14, 0);
    }

    if (!s_price_labels[idx]) {
        s_price_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_align(s_price_labels[idx], LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(s_price_labels[idx], "---.--");
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_price_labels[idx], &lv_font_noto_tc_16, 0);
    }

    if (!s_change_labels[idx]) {
        s_change_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_align(s_change_labels[idx], LV_ALIGN_RIGHT_MID, -4, 0);
        lv_label_set_text(s_change_labels[idx], "+/-");
        lv_obj_set_style_text_color(s_change_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_change_labels[idx], &lv_font_noto_tc_14, 0);
    }
}

static void render_current_page(void)
{
    int total_pages = (s_card_count + DASHBOARD_VISIBLE_ROWS - 1) / DASHBOARD_VISIBLE_ROWS;
    if (total_pages <= 0) {
        total_pages = 1;
    }
    if (s_cur_page >= total_pages) {
        s_cur_page = 0;
    }

    int base = s_cur_page * DASHBOARD_VISIBLE_ROWS;
    for (int row = 0; row < DASHBOARD_VISIBLE_ROWS; row++) {
        int data_idx = base + row;
        if (data_idx < s_card_count) {
            ensure_card_widgets(row);
            lv_obj_clear_flag(s_cards[row], LV_OBJ_FLAG_HIDDEN);
            if (s_cached_valid[data_idx]) {
                apply_card_widgets(row, &s_cached_quotes[data_idx]);
            } else {
                lv_label_set_text(s_name_labels[row], "---");
                lv_label_set_text(s_price_labels[row], "---.--");
                lv_label_set_text(s_change_labels[row], "+/-");
                lv_obj_set_style_bg_color(s_cards[row], COLOR_CARD, 0);
            }
        } else {
            lv_obj_add_flag(s_cards[row], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void page_flip_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_card_list || s_card_count <= DASHBOARD_VISIBLE_ROWS) {
        return;
    }

    int total_pages = (s_card_count + DASHBOARD_VISIBLE_ROWS - 1) / DASHBOARD_VISIBLE_ROWS;
    if (total_pages <= 1) {
        return;
    }

    s_cur_page = (s_cur_page + 1) % total_pages;
    render_current_page();
}

lv_obj_t *screen_dashboard_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_card_list = lv_obj_create(s_screen);
    lv_obj_set_pos(s_card_list, 4, 24);
    lv_obj_set_size(s_card_list, LCD_WIDTH - 8, 182);
    lv_obj_set_style_bg_opa(s_card_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_card_list, 0, 0);
    lv_obj_set_style_pad_all(s_card_list, 0, 0);
    lv_obj_set_scrollbar_mode(s_card_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_card_list, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 股票卡片列表 ---- */
    for (int i = 0; i < DASHBOARD_VISIBLE_ROWS; i++) {
        s_cards[i] = lv_obj_create(s_card_list);
        lv_obj_set_size(s_cards[i], LCD_WIDTH - 8, 32);
        lv_obj_set_pos(s_cards[i], 0, i * 36);
        lv_obj_set_style_bg_color(s_cards[i], COLOR_CARD, 0);
        lv_obj_set_style_radius(s_cards[i], 4, 0);
        lv_obj_set_style_pad_all(s_cards[i], 0, 0);
        lv_obj_clear_flag(s_cards[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_empty_label = lv_label_create(s_card_list);
    lv_label_set_text(s_empty_label, "No stocks configured.\nGo to Settings to add.");
    lv_obj_align(s_empty_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_empty_label, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_align(s_empty_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_empty_label, &lv_font_noto_tc_14, 0);

    /* 最後更新時間 */
    s_update_label = lv_label_create(s_screen);
    lv_obj_set_pos(s_update_label, 4, 207);
    lv_label_set_text(s_update_label, "更新: --:--:--");
    lv_obj_set_style_text_color(s_update_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_update_label, &lv_font_noto_tc_14, 0);

    s_next_label = lv_label_create(s_screen);
    lv_obj_set_pos(s_next_label, 4, 220);
    lv_label_set_text(s_next_label, "Next: --:--:--");
    lv_obj_set_style_text_color(s_next_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_next_label, &lv_font_montserrat_10, 0);

    s_page_flip_timer = lv_timer_create(page_flip_timer_cb, DASHBOARD_PAGE_FLIP_S * 1000, NULL);

    return s_screen;
}

/* 將一筆報價套用到對應的卡片 LVGL widget（只在 active 時呼叫）*/
static void apply_card_widgets(int idx, const stock_quote_t *q)
{
    ensure_card_widgets(idx);
    lv_label_set_text_fmt(s_name_labels[idx], "%s\n%s", q->symbol, q->name);

    char price_str[16];
    if (q->is_market_closed) {
        snprintf(price_str, sizeof(price_str), "%.2f\n休市", q->yesterday_close);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_hex(0x888888), 0);
    } else {
        snprintf(price_str, sizeof(price_str), "%.2f", q->current_price);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
    }
    lv_label_set_text(s_price_labels[idx], price_str);

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

    char time_str[40];
    snprintf(time_str, sizeof(time_str), "更新: %s", q->trade_time);
    lv_label_set_text(s_update_label, time_str);

    uint32_t remaining_s = scheduler_get_seconds_to_next_quote();
    time_t now = time(NULL);
    time_t next_time = (now > 0) ? (now + (time_t)remaining_s) : 0;
    struct tm next_tm;
    char next_str[24];
    if (next_time > 0 && localtime_r(&next_time, &next_tm) != NULL) {
        snprintf(next_str, sizeof(next_str), "Next: %02d:%02d:%02d",
                 next_tm.tm_hour, next_tm.tm_min, next_tm.tm_sec);
    } else {
        snprintf(next_str, sizeof(next_str), "Next: --:--:--");
    }
    lv_label_set_text(s_next_label, next_str);
}

void screen_dashboard_set_card_count(uint8_t n)
{
    int visible_count = (n <= MAX_STOCK_COUNT) ? (int)n : MAX_STOCK_COUNT;
    s_card_count = visible_count;
    s_cur_page = 0;

    for (int i = visible_count; i < MAX_STOCK_COUNT; i++) {
        s_symbols_order[i][0] = '\0';
        s_cached_valid[i] = false;
    }

    if (visible_count == 0) {
        lv_obj_clear_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < DASHBOARD_VISIBLE_ROWS; i++) {
            lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
        render_current_page();
    }

    if (s_card_list) {
        lv_obj_scroll_to_y(s_card_list, 0, LV_ANIM_OFF);
    }
    if (s_page_flip_timer) {
        lv_timer_reset(s_page_flip_timer);
    }
}

void screen_dashboard_update(const stock_quote_t *q)
{
    if (s_card_count == 0) {
        screen_dashboard_set_card_count(1);
    }

    /* 找對應的卡片 index */
    int idx = -1;
    for (int i = 0; i < s_card_count; i++) {
        if (strcmp(s_symbols_order[i], q->symbol) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        for (int i = 0; i < s_card_count; i++) {
            if (s_symbols_order[i][0] == '\0') {
                idx = i;
                strncpy(s_symbols_order[i], q->symbol, 7);
                s_symbols_order[i][7] = '\0';
                break;
            }
        }
    }
    if (idx == -1 && s_card_count < MAX_STOCK_COUNT) {
        screen_dashboard_set_card_count((uint8_t)(s_card_count + 1));
        idx = s_card_count - 1;
        strncpy(s_symbols_order[idx], q->symbol, 7);
        s_symbols_order[idx][7] = '\0';
    }
    if (idx < 0 || idx >= MAX_STOCK_COUNT) return;

    /* 永遠快取最新資料 */
    s_cached_quotes[idx] = *q;
    s_cached_valid[idx]  = true;

    /* 只有在 dashboard 是 active screen 時才更新 LVGL widget，
     * 避免對非 active screen 的 invalidate 造成畫面閃爍 */
    if (lv_scr_act() != s_screen) return;

    render_current_page();
}

/* 切回 dashboard 時呼叫，把快取資料一次套用到所有卡片 */
void screen_dashboard_refresh(void)
{
    render_current_page();
}
