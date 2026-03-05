#include "screen_dashboard.h"
#include "app_config.h"
#include "scheduler.h"
#include "twse_models.h"
#include "ui_compat.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ====== 顏色定義 ======
 * 注意：Core2 這批面板在目前驅動設定下，標準 RGB hex 的實際顯色會偏移；
 * 下列為實機校正值（目標顯示：漲=紅、跌=綠、平=灰）。
 * 若後續更動 LCD color order / swap 設定，需重新校正這三個值。 */
#define COLOR_UP    lv_color_hex(0x00C4CF)   /* 漲：紅（實機校正） */
#define COLOR_DOWN  lv_color_hex(0x8864B0)   /* 跌：綠（實機校正） */
#define COLOR_FLAT  lv_color_hex(0x7A7A7A)   /* 平：灰（實機校正，避免偏綠） */
#define COLOR_BG    lv_color_hex(0x1A1A2E)   /* 深藍背景 */
#define COLOR_CARD  lv_color_hex(0x1C2A4A)   /* 卡片背景 */
#define DASHBOARD_VISIBLE_ROWS 5

static lv_obj_t *s_screen  = NULL;
static lv_obj_t *s_card_list = NULL;
static lv_obj_t *s_cards[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_name_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_industry_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_price_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_change_labels[DASHBOARD_VISIBLE_ROWS] = {NULL};
static lv_obj_t *s_empty_label = NULL;
static lv_obj_t *s_update_label = NULL;
static lv_obj_t *s_next_label = NULL;
static lv_timer_t *s_rotation_timer = NULL;
static const char *TAG = "screen_dashboard";

/* 快取最新報價，供非 active 狀態下累積，切回 dashboard 時重新套用 */
static char          s_symbols_order[MAX_STOCK_COUNT][8] = {0};
static int           s_card_count = 0;
static stock_quote_t s_cached_quotes[MAX_STOCK_COUNT];
static bool          s_cached_valid[MAX_STOCK_COUNT] = {false};
static stock_quote_t s_reordered_quotes[MAX_STOCK_COUNT];
static bool          s_reordered_valid[MAX_STOCK_COUNT] = {false};
static uint32_t      s_last_missing_log_ms[MAX_STOCK_COUNT] = {0};
static int           s_slot_stock_idx[DASHBOARD_VISIBLE_ROWS] = {0};
static int           s_next_slot_idx = 0;
static int           s_next_stock_idx = 0;

static void apply_card_widgets(int idx, const stock_quote_t *q);
static void render_dashboard_slots(void);
static bool is_limit_up_price(float price, const stock_quote_t *q);
static bool is_limit_down_price(float price, const stock_quote_t *q);

#define CARD_COL_COUNT    4
#define CARD_LEFT_PAD     4
#define CARD_RIGHT_PAD    4
#define CARD_COL_GAP      4

static lv_coord_t s_col_x[CARD_COL_COUNT] = {0};
static lv_coord_t s_col_w[CARD_COL_COUNT] = {0};
static bool s_layout_ready = false;

static bool float_nearly_equal(float a, float b)
{
    return fabsf(a - b) <= 0.0005f;
}

static bool is_limit_up_price(float price, const stock_quote_t *q)
{
    return q && q->has_limit_bounds && float_nearly_equal(price, q->limit_up_price);
}

static bool is_limit_down_price(float price, const stock_quote_t *q)
{
    return q && q->has_limit_bounds && float_nearly_equal(price, q->limit_down_price);
}

static void compute_card_layout(void)
{
    lv_coord_t card_w = (LCD_WIDTH - 8);
    if (s_cards[0]) {
        card_w = lv_obj_get_width(s_cards[0]);
    }

    lv_coord_t content_w = card_w - CARD_LEFT_PAD - CARD_RIGHT_PAD - (CARD_COL_GAP * (CARD_COL_COUNT - 1));
    if (content_w < 220) {
        content_w = 220;
    }

    lv_coord_t w_name = 74;
    lv_coord_t w_price = 64;
    lv_coord_t w_change = 66;
    lv_coord_t used = w_name + w_price + w_change;
    lv_coord_t w_industry = content_w - used;
    if (w_industry < 0) {
        w_industry = 0;
    }

    s_col_w[0] = w_name;
    s_col_w[1] = w_price;
    s_col_w[2] = w_change;
    s_col_w[3] = w_industry;

    s_col_x[0] = CARD_LEFT_PAD;
    s_col_x[1] = s_col_x[0] + s_col_w[0] + CARD_COL_GAP;
    s_col_x[2] = s_col_x[1] + s_col_w[1] + CARD_COL_GAP;
    s_col_x[3] = s_col_x[2] + s_col_w[2] + CARD_COL_GAP;
    s_layout_ready = true;
}

static void ensure_card_widgets(int idx)
{
    if (idx < 0 || idx >= DASHBOARD_VISIBLE_ROWS) {
        return;
    }
    if (!s_layout_ready) {
        compute_card_layout();
    }

    if (!s_name_labels[idx]) {
        s_name_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_set_pos(s_name_labels[idx], s_col_x[0], 0);
        lv_obj_set_size(s_name_labels[idx], s_col_w[0], 32);
        lv_label_set_long_mode(s_name_labels[idx], LV_LABEL_LONG_CLIP);
        lv_label_set_text(s_name_labels[idx], "---");
        lv_obj_set_style_text_color(s_name_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_name_labels[idx], &lv_font_noto_tc_14, 0);
        lv_obj_set_style_text_line_space(s_name_labels[idx], 0, 0);
    }

    if (!s_industry_labels[idx]) {
        s_industry_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_set_pos(s_industry_labels[idx], s_col_x[3], 0);
        lv_obj_set_size(s_industry_labels[idx], s_col_w[3], 32);
        lv_label_set_long_mode(s_industry_labels[idx], LV_LABEL_LONG_WRAP);
        lv_label_set_text(s_industry_labels[idx], "");
        lv_obj_set_style_text_color(s_industry_labels[idx], lv_color_hex(0xC7D2E0), 0);
        lv_obj_set_style_text_font(s_industry_labels[idx], &lv_font_noto_tc_14, 0);
        lv_obj_set_style_text_line_space(s_industry_labels[idx], 0, 0);
        lv_obj_set_style_text_align(s_industry_labels[idx], LV_TEXT_ALIGN_RIGHT, 0);
    }

    if (!s_price_labels[idx]) {
        s_price_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_set_pos(s_price_labels[idx], s_col_x[1] - 2, 4);
        lv_obj_set_size(s_price_labels[idx], s_col_w[1] + 4, 22);
        lv_label_set_long_mode(s_price_labels[idx], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(s_price_labels[idx], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(s_price_labels[idx], "---.--");
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_price_labels[idx], &lv_font_montserrat_14, 0);
        lv_obj_set_style_bg_opa(s_price_labels[idx], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(s_price_labels[idx], 4, 0);
        lv_obj_set_style_pad_left(s_price_labels[idx], 4, 0);
        lv_obj_set_style_pad_right(s_price_labels[idx], 4, 0);
        lv_obj_set_style_pad_top(s_price_labels[idx], 2, 0);
        lv_obj_set_style_pad_bottom(s_price_labels[idx], 2, 0);
    }

    if (!s_change_labels[idx]) {
        s_change_labels[idx] = lv_label_create(s_cards[idx]);
        lv_obj_set_pos(s_change_labels[idx], s_col_x[2], 6);
        lv_obj_set_size(s_change_labels[idx], s_col_w[2], 16);
        lv_label_set_long_mode(s_change_labels[idx], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(s_change_labels[idx], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(s_change_labels[idx], "+/-");
        lv_obj_set_style_text_color(s_change_labels[idx], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_change_labels[idx], &lv_font_montserrat_10, 0);
    }
}

static void reset_slot_rotation_state(void)
{
    for (int slot = 0; slot < DASHBOARD_VISIBLE_ROWS; slot++) {
        if (slot < s_card_count) {
            s_slot_stock_idx[slot] = slot;
        } else {
            s_slot_stock_idx[slot] = -1;
        }
    }

    s_next_slot_idx = 0;
    if (s_card_count > DASHBOARD_VISIBLE_ROWS) {
        s_next_stock_idx = DASHBOARD_VISIBLE_ROWS % s_card_count;
    } else {
        s_next_stock_idx = 0;
    }
}

static void render_slot_with_stock(int slot, int stock_idx)
{
    if (slot < 0 || slot >= DASHBOARD_VISIBLE_ROWS) {
        return;
    }

    if (stock_idx < 0 || stock_idx >= s_card_count) {
        lv_obj_add_flag(s_cards[slot], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ensure_card_widgets(slot);
    lv_obj_clear_flag(s_cards[slot], LV_OBJ_FLAG_HIDDEN);
    if (s_cached_valid[stock_idx]) {
        apply_card_widgets(slot, &s_cached_quotes[stock_idx]);
    } else {
        uint32_t now_ms = (uint32_t)esp_log_timestamp();
        if (stock_idx >= 0 && stock_idx < MAX_STOCK_COUNT &&
            now_ms - s_last_missing_log_ms[stock_idx] >= 30000U) {
            ESP_LOGW(TAG, "missing quote symbol=%s slot=%d idx=%d",
                     s_symbols_order[stock_idx], slot, stock_idx);
            s_last_missing_log_ms[stock_idx] = now_ms;
        }
        if (s_symbols_order[stock_idx][0] != '\0') {
            lv_label_set_text_fmt(s_name_labels[slot], "%s\n--", s_symbols_order[stock_idx]);
        } else {
            lv_label_set_text(s_name_labels[slot], "---");
        }
        lv_label_set_text(s_industry_labels[slot], "");
        lv_label_set_text(s_price_labels[slot], "N/A");
        lv_label_set_text(s_change_labels[slot], "--");
        lv_obj_set_style_bg_opa(s_price_labels[slot], LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_price_labels[slot], lv_color_white(), 0);
        lv_obj_set_style_text_color(s_change_labels[slot], lv_color_white(), 0);
        lv_obj_set_style_border_color(s_cards[slot], COLOR_CARD, 0);
        lv_obj_set_style_bg_color(s_cards[slot], COLOR_CARD, 0);
    }
}

static void render_dashboard_slots(void)
{
    for (int slot = 0; slot < DASHBOARD_VISIBLE_ROWS; slot++) {
        render_slot_with_stock(slot, s_slot_stock_idx[slot]);
    }
}

static void rotation_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_card_list || s_card_count <= DASHBOARD_VISIBLE_ROWS) {
        return;
    }
    if (lv_scr_act() != s_screen) {
        return;
    }

    int slot = s_next_slot_idx;
    int stock_idx = s_next_stock_idx;
    s_slot_stock_idx[slot] = stock_idx;
    render_slot_with_stock(slot, stock_idx);

    s_next_slot_idx = (s_next_slot_idx + 1) % DASHBOARD_VISIBLE_ROWS;
    s_next_stock_idx = (s_next_stock_idx + 1) % s_card_count;
}

lv_obj_t *screen_dashboard_create(void)
{
    const lv_coord_t card_list_x = 4;
    const lv_coord_t card_list_y = UI_CONTENT_TOP_Y;
    const lv_coord_t card_h = 39;
    const lv_coord_t card_pitch = 40;
    const lv_coord_t card_list_h = (DASHBOARD_VISIBLE_ROWS - 1) * card_pitch + card_h;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_card_list = lv_obj_create(s_screen);
    lv_obj_set_pos(s_card_list, card_list_x, card_list_y);
    lv_obj_set_size(s_card_list, LCD_WIDTH - 8, card_list_h);
    lv_obj_set_style_bg_opa(s_card_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_card_list, 0, 0);
    lv_obj_set_style_pad_all(s_card_list, 0, 0);
    lv_obj_set_scrollbar_mode(s_card_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_card_list, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 股票卡片列表 ---- */
    for (int i = 0; i < DASHBOARD_VISIBLE_ROWS; i++) {
        s_cards[i] = lv_obj_create(s_card_list);
        lv_obj_set_size(s_cards[i], LCD_WIDTH - 8, card_h);
        lv_obj_set_pos(s_cards[i], 0, i * card_pitch);
        lv_obj_set_style_bg_color(s_cards[i], COLOR_CARD, 0);
        lv_obj_set_style_radius(s_cards[i], 4, 0);
        lv_obj_set_style_pad_all(s_cards[i], 0, 0);
        lv_obj_set_style_border_side(s_cards[i], LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(s_cards[i], 4, 0);
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
    lv_obj_set_pos(s_update_label, 4, card_list_y + card_list_h + 1);
    lv_label_set_text(s_update_label, "Updated --:--:--");
    lv_obj_set_style_text_color(s_update_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_update_label, &lv_font_noto_tc_14, 0);

    s_next_label = lv_label_create(s_screen);
    lv_obj_align(s_next_label, LV_ALIGN_TOP_RIGHT, -4, card_list_y + card_list_h + 1);
    lv_label_set_text(s_next_label, "Next Time: --:--:--");
    lv_obj_set_style_text_color(s_next_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_next_label, &lv_font_montserrat_10, 0);

    reset_slot_rotation_state();
    s_rotation_timer = lv_timer_create(rotation_timer_cb, DASHBOARD_SLOT_ROTATION_MS, NULL);

    return s_screen;
}

/* 將一筆報價套用到對應的卡片 LVGL widget（只在 active 時呼叫）*/
static void apply_card_widgets(int idx, const stock_quote_t *q)
{
    ensure_card_widgets(idx);
    lv_label_set_text_fmt(s_name_labels[idx], "%s\n%s", q->symbol, q->name);
    lv_label_set_text(s_industry_labels[idx], q->industry);

    char price_str[16];
    float display_price = 0.0f;
    lv_color_t accent = (q->change_percent > 0.01f) ? COLOR_UP :
                        (q->change_percent < -0.01f) ? COLOR_DOWN :
                        COLOR_FLAT;
    if (q->is_market_closed) {
        display_price = q->yesterday_close;
    } else {
        display_price = q->current_price;
    }
    snprintf(price_str, sizeof(price_str), "%.2f", display_price);
    bool limit_up = is_limit_up_price(display_price, q);
    bool limit_down = is_limit_down_price(display_price, q);

    if (limit_up) {
        lv_obj_set_style_bg_color(s_price_labels[idx], COLOR_UP, 0);
        lv_obj_set_style_bg_opa(s_price_labels[idx], LV_OPA_40, 0);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
    } else if (limit_down) {
        lv_obj_set_style_bg_color(s_price_labels[idx], COLOR_DOWN, 0);
        lv_obj_set_style_bg_opa(s_price_labels[idx], LV_OPA_40, 0);
        lv_obj_set_style_text_color(s_price_labels[idx], lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_opa(s_price_labels[idx], LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_price_labels[idx], accent, 0);
    }
    lv_label_set_text(s_price_labels[idx], price_str);

    char change_str[16];
    float change_abs = fabsf(q->change_percent);
    if (q->change_percent > 0.01f) {
        snprintf(change_str, sizeof(change_str), "↗%.2f%%", change_abs);
    } else if (q->change_percent < -0.01f) {
        snprintf(change_str, sizeof(change_str), "↘%.2f%%", change_abs);
    } else {
        snprintf(change_str, sizeof(change_str), "－0.00%%");
    }
    lv_label_set_text(s_change_labels[idx], change_str);

    lv_obj_set_style_text_color(s_change_labels[idx], accent, 0);
    lv_obj_set_style_border_color(s_cards[idx], accent, 0);
    lv_obj_set_style_bg_color(s_cards[idx], COLOR_CARD, 0);

    char time_str[40];
    snprintf(time_str, sizeof(time_str), "Updated %s", q->trade_time);
    lv_label_set_text(s_update_label, time_str);

    uint32_t remaining_s = scheduler_get_seconds_to_next_quote();
    time_t now = time(NULL);
    time_t next_time = (now > 0) ? (now + (time_t)remaining_s) : 0;
    struct tm next_tm;
    char next_str[24];
    if (next_time > 0 && localtime_r(&next_time, &next_tm) != NULL) {
        snprintf(next_str, sizeof(next_str), "Next Time: %02d:%02d:%02d",
                 next_tm.tm_hour, next_tm.tm_min, next_tm.tm_sec);
    } else {
        snprintf(next_str, sizeof(next_str), "Next Time: --:--:--");
    }
    lv_label_set_text(s_next_label, next_str);
}

void screen_dashboard_set_card_count(uint8_t n)
{
    int visible_count = (n <= MAX_STOCK_COUNT) ? (int)n : MAX_STOCK_COUNT;
    s_card_count = visible_count;
    reset_slot_rotation_state();

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
        render_dashboard_slots();
    }

    if (s_card_list) {
        lv_obj_scroll_to_y(s_card_list, 0, LV_ANIM_OFF);
    }
    if (s_rotation_timer) {
        lv_timer_reset(s_rotation_timer);
    }
}

void screen_dashboard_set_symbols(const char symbols[][8], uint8_t count)
{
    int next_count = (count <= MAX_STOCK_COUNT) ? (int)count : MAX_STOCK_COUNT;
    char old_symbols[MAX_STOCK_COUNT][8] = {0};
    int old_to_new[MAX_STOCK_COUNT];

    memcpy(old_symbols, s_symbols_order, sizeof(old_symbols));
    for (int i = 0; i < MAX_STOCK_COUNT; i++) {
        old_to_new[i] = -1;
    }
    memset(s_reordered_quotes, 0, sizeof(s_reordered_quotes));
    memset(s_reordered_valid, 0, sizeof(s_reordered_valid));

    for (int i = 0; i < next_count; i++) {
        s_symbols_order[i][0] = '\0';
        if (symbols && symbols[i][0] != '\0') {
            strlcpy(s_symbols_order[i], symbols[i], sizeof(s_symbols_order[i]));
        }

        for (int j = 0; j < MAX_STOCK_COUNT; j++) {
            if (s_cached_valid[j] && strcmp(old_symbols[j], s_symbols_order[i]) == 0) {
                old_to_new[j] = i;
                break;
            }
        }
    }

    for (int i = next_count; i < MAX_STOCK_COUNT; i++) {
        s_symbols_order[i][0] = '\0';
    }

    for (int old_idx = 0; old_idx < MAX_STOCK_COUNT; old_idx++) {
        int new_idx = old_to_new[old_idx];
        if (new_idx >= 0 && new_idx < MAX_STOCK_COUNT) {
            s_reordered_quotes[new_idx] = s_cached_quotes[old_idx];
            s_reordered_valid[new_idx] = true;
        }
    }

    memcpy(s_cached_quotes, s_reordered_quotes, sizeof(s_cached_quotes));
    memcpy(s_cached_valid, s_reordered_valid, sizeof(s_cached_valid));

    screen_dashboard_set_card_count((uint8_t)next_count);
}

void screen_dashboard_update(const stock_quote_t *q)
{
    if (s_card_count == 0) {
        screen_dashboard_set_card_count(1);
    }
    const char *mapping_action = "match";

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
                strlcpy(s_symbols_order[i], q->symbol, sizeof(s_symbols_order[i]));
                mapping_action = "assign_empty";
                break;
            }
        }
    }
    if (idx == -1 && s_card_count < MAX_STOCK_COUNT) {
        screen_dashboard_set_card_count((uint8_t)(s_card_count + 1));
        idx = s_card_count - 1;
        strlcpy(s_symbols_order[idx], q->symbol, sizeof(s_symbols_order[idx]));
        mapping_action = "expand";
    }
    if (idx < 0 || idx >= MAX_STOCK_COUNT) {
        ESP_LOGW(TAG, "ui_quote symbol=%s -> drop idx=%d action=%s",
                 q->symbol, idx, mapping_action);
        return;
    }

    ESP_LOGI(TAG,
             "ui_quote symbol=%s idx=%d action=%s price=%.2f chg=%.2f%% valid=%d closed=%d",
             q->symbol, idx, mapping_action,
             q->current_price, q->change_percent,
             q->is_valid ? 1 : 0, q->is_market_closed ? 1 : 0);

    /* 永遠快取最新資料 */
    s_cached_quotes[idx] = *q;
    s_cached_valid[idx]  = true;

    /* 只有在 dashboard 是 active screen 時才更新 LVGL widget，
     * 避免對非 active screen 的 invalidate 造成畫面閃爍 */
    if (lv_scr_act() != s_screen) return;

    if (s_card_count <= DASHBOARD_VISIBLE_ROWS) {
        render_dashboard_slots();
        return;
    }

    for (int slot = 0; slot < DASHBOARD_VISIBLE_ROWS; slot++) {
        if (s_slot_stock_idx[slot] == idx) {
            render_slot_with_stock(slot, idx);
        }
    }
}

/* 切回 dashboard 時呼叫，把快取資料一次套用到所有卡片 */
void screen_dashboard_refresh(void)
{
    render_dashboard_slots();
}

void screen_dashboard_on_enter(void)
{
    reset_slot_rotation_state();
    render_dashboard_slots();
    if (s_rotation_timer) {
        lv_timer_reset(s_rotation_timer);
    }
}

void screen_dashboard_on_leave(void)
{
}
