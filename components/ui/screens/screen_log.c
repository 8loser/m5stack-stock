#include "screen_log.h"
#include "app_config.h"
#include "rtc_bm8563.h"
#include "freertos/FreeRTOS.h"
#include "ui_compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define COLOR_BG            lv_color_hex(0x101826)
#define LOG_RING_SIZE       8
#define LOG_MSG_MAX         56
#define LOG_VISIBLE_LINES   LOG_RING_SIZE
#define LOG_TS_MAX          20
#define LOG_FIRST_LINE_COLS 38
#define LOG_NEXT_LINE_COLS  34
#define LOG_CONT_INDENT     "    "

typedef struct {
    log_tag_t tag;
    log_level_t level;
    char ts[LOG_TS_MAX];
    char msg[LOG_MSG_MAX];
} log_entry_t;

static log_entry_t s_ring[LOG_RING_SIZE];
static int s_head = 0;
static int s_count = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static lv_obj_t *s_labels[LOG_VISIBLE_LINES] = {0};
static lv_obj_t *s_log_list = NULL;

static const char *log_tag_to_str(log_tag_t tag);

static void log_fill_timestamp(char *out, size_t out_len)
{
    rtc_time_t rt = {0};
    if (rtc_bm8563_get_time(&rt) == ESP_OK &&
        rt.year >= 2000 && rt.year <= 2099 &&
        rt.month >= 1 && rt.month <= 12 &&
        rt.day >= 1 && rt.day <= 31 &&
        rt.hours < 24 && rt.minutes < 60 && rt.seconds < 60) {
        snprintf(out, out_len, "%04u-%02u-%02u %02u:%02u:%02u",
                 rt.year, rt.month, rt.day, rt.hours, rt.minutes, rt.seconds);
        return;
    }

    time_t now = time(NULL);
    if (now > 0) {
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        strftime(out, out_len, "%Y-%m-%d %H:%M:%S", &tm_info);
        return;
    }

    snprintf(out, out_len, "0000-00-00 00:00:00");
}

static void log_format_entry_text(const log_entry_t *entry, char *out, size_t out_len)
{
    const char *msg = entry->msg;
    size_t msg_len = strlen(msg);
    size_t pos = 0;
    int wrote = snprintf(out, out_len, "[%s] %s ", log_tag_to_str(entry->tag), entry->ts);
    if (wrote < 0 || (size_t)wrote >= out_len) {
        if (out_len > 0) {
            out[out_len - 1] = '\0';
        }
        return;
    }

    size_t used = (size_t)wrote;
    size_t first_chunk = (used < LOG_FIRST_LINE_COLS) ? (LOG_FIRST_LINE_COLS - used) : 1;
    if (first_chunk > msg_len) {
        first_chunk = msg_len;
    }

    if (first_chunk > 0) {
        wrote = snprintf(out + used, out_len - used, "%.*s", (int)first_chunk, msg);
        if (wrote < 0 || (size_t)wrote >= out_len - used) {
            out[out_len - 1] = '\0';
            return;
        }
        used += (size_t)wrote;
        pos += first_chunk;
    }

    while (pos < msg_len && used + 1 < out_len) {
        size_t chunk = msg_len - pos;
        if (chunk > LOG_NEXT_LINE_COLS) {
            chunk = LOG_NEXT_LINE_COLS;
        }

        wrote = snprintf(out + used, out_len - used, "\n%s%.*s",
                         LOG_CONT_INDENT, (int)chunk, msg + pos);
        if (wrote < 0 || (size_t)wrote >= out_len - used) {
            out[out_len - 1] = '\0';
            return;
        }
        used += (size_t)wrote;
        pos += chunk;
    }
}

static const lv_font_t *log_line_font(void)
{
    return &lv_font_noto_tc_14;
}

static const char *log_tag_to_str(log_tag_t tag)
{
    switch (tag) {
    case LOG_TAG_STOCK: return "抓價";
    case LOG_TAG_WIFI:  return "WiFi";
    case LOG_TAG_AI:    return "AI";
    case LOG_TAG_SYS:   return "系統";
    default:            return "系統";
    }
}

static lv_color_t log_color_for(log_tag_t tag, log_level_t level)
{
    if (level == LOG_LEVEL_ERROR) {
        return lv_color_hex(0xEF5350);
    }
    if (level == LOG_LEVEL_WARN) {
        return lv_color_hex(0xFFB74D);
    }

    switch (tag) {
    case LOG_TAG_STOCK: return lv_color_hex(0x4FC3F7);
    case LOG_TAG_WIFI:  return lv_color_hex(0x81C784);
    case LOG_TAG_AI:    return lv_color_hex(0xCE93D8);
    case LOG_TAG_SYS:   return lv_color_hex(0xB0BEC5);
    default:            return lv_color_hex(0xB0BEC5);
    }
}

void screen_log_push(log_tag_t tag, log_level_t level, const char *msg)
{
    const char *safe_msg = (msg != NULL) ? msg : "";
    char ts[LOG_TS_MAX];
    log_fill_timestamp(ts, sizeof(ts));

    portENTER_CRITICAL(&s_mux);
    log_entry_t *entry = &s_ring[s_head];
    entry->tag = tag;
    entry->level = level;
    strlcpy(entry->ts, ts, sizeof(entry->ts));
    strlcpy(entry->msg, safe_msg, sizeof(entry->msg));

    s_head = (s_head + 1) % LOG_RING_SIZE;
    if (s_count < LOG_RING_SIZE) {
        s_count++;
    }
    portEXIT_CRITICAL(&s_mux);
}

lv_obj_t *screen_log_create(void)
{
    const lv_coord_t log_list_y = UI_CONTENT_TOP_Y + 2;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    s_log_list = lv_obj_create(screen);
    lv_obj_set_pos(s_log_list, 6, log_list_y);
    lv_obj_set_size(s_log_list, LCD_WIDTH - 12, LCD_HEIGHT - log_list_y - 4);
    lv_obj_set_style_bg_opa(s_log_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_log_list, 0, 0);
    lv_obj_set_style_pad_all(s_log_list, 0, 0);
    lv_obj_set_style_pad_row(s_log_list, 4, 0);
    lv_obj_set_scroll_dir(s_log_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_log_list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(s_log_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_log_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        s_labels[i] = lv_label_create(s_log_list);
        lv_obj_set_width(s_labels[i], LCD_WIDTH - 24);
        lv_label_set_long_mode(s_labels[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(s_labels[i], log_line_font(), 0);
        lv_obj_set_style_text_color(s_labels[i], lv_color_hex(0xB0BEC5), 0);
        lv_obj_set_style_text_line_space(s_labels[i], 2, 0);
        lv_label_set_text(s_labels[i], "");
    }

    return screen;
}

void screen_log_refresh(void)
{
    if (s_labels[0] == NULL) {
        return;
    }

    log_entry_t latest[LOG_VISIBLE_LINES];
    int latest_count = 0;

    portENTER_CRITICAL(&s_mux);
    latest_count = (s_count < LOG_VISIBLE_LINES) ? s_count : LOG_VISIBLE_LINES;
    for (int i = 0; i < latest_count; i++) {
        int idx = (s_head - 1 - i + LOG_RING_SIZE) % LOG_RING_SIZE;
        latest[i] = s_ring[idx];
    }
    portEXIT_CRITICAL(&s_mux);

    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        if (i < latest_count) {
            char line[196];
            log_format_entry_text(&latest[i], line, sizeof(line));
            lv_label_set_text(s_labels[i], line);
            lv_obj_set_style_text_color(
                s_labels[i], log_color_for(latest[i].tag, latest[i].level), 0);
        } else {
            lv_label_set_text(s_labels[i], "");
        }
    }

    if (s_log_list != NULL) {
        lv_obj_scroll_to_y(s_log_list, 0, LV_ANIM_OFF);
    }
}
