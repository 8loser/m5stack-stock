#include "twse_client.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "twse";

#define HTTP_BUF_INIT_SIZE  4096
#define HTTP_BUF_MAX_SIZE   32768

static QueueHandle_t s_queue        = NULL;
static TaskHandle_t  s_task_handle  = NULL;
static bool          s_task_running = false;

static const char *extract_symbol_from_ch(const char *ch)
{
    static char symbol[8];
    if (!ch || ch[0] == '\0') return NULL;

    const char *underscore = strchr(ch, '_');
    if (!underscore || underscore[1] == '\0') return NULL;

    const char *start = underscore + 1;
    const char *end = strchr(start, '.');
    if (!end || end <= start) return NULL;

    size_t len = (size_t)(end - start);
    if (len == 0 || len >= sizeof(symbol)) return NULL;

    memcpy(symbol, start, len);
    symbol[len] = '\0';
    return symbol;
}

static bool parse_first_book_price(cJSON *item, const char *key, float *out_price)
{
    if (!item || !key || !out_price) return false;
    cJSON *j = cJSON_GetObjectItem(item, key);
    if (!j || !cJSON_IsString(j) || !j->valuestring) return false;
    if (strcmp(j->valuestring, "-") == 0 || j->valuestring[0] == '\0') return false;

    /* TWSE orderbook format: "64.0000_63.9000_..." */
    char buf[32] = {0};
    size_t i = 0;
    while (j->valuestring[i] != '\0' &&
           j->valuestring[i] != '_' &&
           i < sizeof(buf) - 1) {
        buf[i] = j->valuestring[i];
        i++;
    }
    buf[i] = '\0';
    if (buf[0] == '\0' || strcmp(buf, "-") == 0) return false;

    *out_price = strtof(buf, NULL);
    return (*out_price > 0.0f);
}

static bool has_numeric_string(cJSON *item, const char *key)
{
    cJSON *j = cJSON_GetObjectItem(item, key);
    return (j && cJSON_IsString(j) && j->valuestring &&
            strcmp(j->valuestring, "-") != 0 && j->valuestring[0] != '\0');
}

static bool volume_is_zero(cJSON *item, long *out_volume)
{
    cJSON *vol = cJSON_GetObjectItem(item, "v");
    long parsed = 0;
    bool has_volume = false;

    if (vol && cJSON_IsString(vol) && vol->valuestring &&
        strcmp(vol->valuestring, "-") != 0 && vol->valuestring[0] != '\0') {
        parsed = strtol(vol->valuestring, NULL, 10);
        has_volume = true;
    } else if (vol && cJSON_IsNumber(vol)) {
        parsed = (long)vol->valuedouble;
        has_volume = true;
    }

    if (out_volume) *out_volume = parsed;
    return has_volume && (parsed <= 0);
}

typedef struct {
    const char *code;
    const char *name;
} industry_map_t;

static const industry_map_t s_industry_map[] = {
    {"1", "水泥工業"},
    {"2", "食品工業"},
    {"3", "塑膠工業"},
    {"4", "紡織纖維"},
    {"5", "電機機械"},
    {"6", "電器電纜"},
    {"8", "玻璃陶瓷"},
    {"9", "造紙工業"},
    {"10", "鋼鐵工業"},
    {"11", "橡膠工業"},
    {"12", "汽車工業"},
    {"14", "建材營造"},
    {"15", "航運業"},
    {"16", "觀光餐旅"},
    {"17", "金融保險"},
    {"18", "貿易百貨"},
    {"19", "綜合"},
    {"20", "其他"},
    {"21", "化學工業"},
    {"22", "生技醫療"},
    {"23", "油電燃氣"},
    {"24", "半導體業"},
    {"25", "電腦及週邊設備業"},
    {"26", "光電業"},
    {"27", "通信網路業"},
    {"28", "電子零組件業"},
    {"29", "電子通路業"},
    {"30", "資訊服務業"},
    {"31", "其他電子業"},
    {"32", "文化創意業"},
    {"33", "農業科技業"},
    {"34", "電子商務"},
    {"35", "綠能環保"},
    {"36", "數位雲端"},
    {"37", "運動休閒"},
    {"38", "居家生活"},
    {"80", "管理股票"},
    {"91", "存託憑證"},
};

static bool copy_json_string(char *dst, size_t dst_size, cJSON *obj, const char *key)
{
    cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j || !cJSON_IsString(j) || !j->valuestring || strcmp(j->valuestring, "-") == 0) {
        return false;
    }
    strlcpy(dst, j->valuestring, dst_size);
    return true;
}

static bool copy_json_text(char *dst, size_t dst_size, cJSON *obj, const char *key)
{
    cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j) return false;

    if (cJSON_IsString(j) && j->valuestring && strcmp(j->valuestring, "-") != 0) {
        strlcpy(dst, j->valuestring, dst_size);
        return true;
    }

    if (cJSON_IsNumber(j)) {
        snprintf(dst, dst_size, "%.0f", j->valuedouble);
        return true;
    }

    return false;
}

static const char *industry_from_code(const char *code)
{
    if (!code || code[0] == '\0') return NULL;

    for (size_t i = 0; i < sizeof(s_industry_map) / sizeof(s_industry_map[0]); i++) {
        if (strcmp(s_industry_map[i].code, code) == 0) {
            return s_industry_map[i].name;
        }
    }
    return NULL;
}

static const char *normalize_industry_code(const char *code, char *out, size_t out_size)
{
    if (!code || !out || out_size == 0) return NULL;
    while (*code == '0' && code[1] != '\0') {
        code++;
    }
    strlcpy(out, code, out_size);
    return out;
}

static void parse_symbol_metadata(cJSON *item, stock_symbol_info_t *out)
{
    /* short_name 優先使用 n；name 優先使用 nf */
    copy_json_string(out->short_name, sizeof(out->short_name), item, "n");
    if (!copy_json_string(out->name, sizeof(out->name), item, "nf")) {
        if (out->short_name[0] != '\0') {
            strlcpy(out->name, out->short_name, sizeof(out->name));
        }
    }

    if (copy_json_string(out->industry, sizeof(out->industry), item, "industry")) {
        return;
    }

    /* 常見為產業代碼 i（可能是字串或數字） */
    char industry_code[12] = {0};
    if (copy_json_text(industry_code, sizeof(industry_code), item, "i")) {
        bool all_digit = true;
        for (size_t i = 0; industry_code[i] != '\0'; i++) {
            if (industry_code[i] < '0' || industry_code[i] > '9') {
                all_digit = false;
                break;
            }
        }

        if (all_digit) {
            char normalized[12] = {0};
            const char *mapped = industry_from_code(
                normalize_industry_code(industry_code, normalized, sizeof(normalized)));
            if (mapped) {
                strlcpy(out->industry, mapped, sizeof(out->industry));
                return;
            }
        } else {
            /* 若 i 已是文字，直接使用 */
            strlcpy(out->industry, industry_code, sizeof(out->industry));
            return;
        }
    }

    out->industry[0] = '\0';
}

/* HTTP 事件回調，用於接收 body */
typedef struct {
    char   *buf;
    size_t  capacity;
    size_t  data_len;
    bool    overflow;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        size_t needed = ctx->data_len + evt->data_len + 1;
        if (needed > ctx->capacity) {
            size_t new_cap = ctx->capacity;
            while (new_cap < needed && new_cap < HTTP_BUF_MAX_SIZE) {
                new_cap *= 2;
            }
            if (new_cap < needed || new_cap > HTTP_BUF_MAX_SIZE) {
                ctx->overflow = true;
                return ESP_OK;
            }
            char *new_buf = realloc(ctx->buf, new_cap);
            if (!new_buf) {
                ctx->overflow = true;
                return ESP_OK;
            }
            ctx->buf = new_buf;
            ctx->capacity = new_cap;
        }
        memcpy(ctx->buf + ctx->data_len, evt->data, evt->data_len);
        ctx->data_len += evt->data_len;
    }
    return ESP_OK;
}

/* 解析單一股票 JSON 物件 */
static void parse_stock_item(cJSON *item, const char *symbol, stock_quote_t *q)
{
    strlcpy(q->symbol, symbol, sizeof(q->symbol));
    q->is_valid = false;
    q->is_market_closed = false;
    q->has_limit_bounds = false;

    cJSON *name = cJSON_GetObjectItem(item, "n");
    if (name && cJSON_IsString(name)) {
        strlcpy(q->name, name->valuestring, sizeof(q->name));
    }

    /* 停板上下限（TWSE: u=漲停, w=跌停） */
    cJSON *limit_up = cJSON_GetObjectItem(item, "u");
    cJSON *limit_down = cJSON_GetObjectItem(item, "w");
    if (limit_up && cJSON_IsString(limit_up) && strcmp(limit_up->valuestring, "-") != 0 &&
        limit_down && cJSON_IsString(limit_down) && strcmp(limit_down->valuestring, "-") != 0) {
        q->limit_up_price = strtof(limit_up->valuestring, NULL);
        q->limit_down_price = strtof(limit_down->valuestring, NULL);
        q->has_limit_bounds = true;
    }

    /* 現價 z */
    cJSON *price = cJSON_GetObjectItem(item, "z");
    cJSON *yday_raw = cJSON_GetObjectItem(item, "y");
    const char *z_raw = (price && cJSON_IsString(price) && price->valuestring) ? price->valuestring : "<null>";
    const char *y_raw = (yday_raw && cJSON_IsString(yday_raw) && yday_raw->valuestring) ? yday_raw->valuestring : "<null>";
    bool has_yday = false;
    if (yday_raw && cJSON_IsString(yday_raw) &&
        strcmp(yday_raw->valuestring, "-") != 0) {
        q->yesterday_close = strtof(yday_raw->valuestring, NULL);
        has_yday = (q->yesterday_close > 0.0f);
    }

    bool has_trade_price = (price && cJSON_IsString(price) &&
                            strcmp(price->valuestring, "-") != 0);
    bool has_open = has_numeric_string(item, "o");
    bool has_high = has_numeric_string(item, "h");
    bool has_low = has_numeric_string(item, "l");
    float bid_price = 0.0f;
    float ask_price = 0.0f;
    bool has_bid_price = parse_first_book_price(item, "b", &bid_price);
    bool has_ask_price = parse_first_book_price(item, "a", &ask_price);
    const char *price_source = "none";
    long parsed_volume = 0;
    bool is_zero_volume = volume_is_zero(item, &parsed_volume);

    if (has_trade_price) {
        q->current_price = strtof(price->valuestring, NULL);
        price_source = "z";
    } else if (has_bid_price) {
        q->current_price = bid_price;
        price_source = "b";
    } else if (has_ask_price) {
        q->current_price = ask_price;
        price_source = "a";
    } else if (has_yday) {
        /* 僅在完全沒有可用價格時才回退昨收。 */
        q->current_price = q->yesterday_close;
        price_source = "y";
    }

#define PARSE_FLOAT(key, field) do { \
    cJSON *_j = cJSON_GetObjectItem(item, key); \
    if (_j && cJSON_IsString(_j) && strcmp(_j->valuestring, "-") != 0) \
        q->field = strtof(_j->valuestring, NULL); \
} while(0)

    PARSE_FLOAT("o", open_price);
    PARSE_FLOAT("h", high_price);
    PARSE_FLOAT("l", low_price);
    PARSE_FLOAT("y", yesterday_close);

    q->volume = parsed_volume;

    cJSON *t = cJSON_GetObjectItem(item, "t");
    if (t && cJSON_IsString(t)) {
        strlcpy(q->trade_time, t->valuestring, sizeof(q->trade_time));
    }

    bool market_closed_snapshot =
        is_zero_volume &&
        !has_trade_price &&
        !has_open &&
        !has_high &&
        !has_low &&
        !has_bid_price &&
        !has_ask_price;

    /* 只要有成交價或買/賣盤價，就視為盤中可更新資料。 */
    if (has_trade_price || has_bid_price || has_ask_price) {
        q->is_valid = true;
        q->is_market_closed = false;
    } else if (market_closed_snapshot && has_yday) {
        q->is_valid = false;
        q->is_market_closed = true;
    } else {
        q->is_valid = false;
        q->is_market_closed = false;
    }

    q->change_amount  = q->current_price - q->yesterday_close;
    q->change_percent = (q->yesterday_close > 0.0f)
                        ? (q->change_amount / q->yesterday_close * 100.0f)
                        : 0.0f;
    ESP_LOGD(TAG,
             "parse symbol=%s z=%s y=%s src=%s vol=%ld zero_vol=%d ohl=%d%d%d ab=%d%d closed_rule=%d -> valid=%d market_closed=%d price=%.2f chg=%.2f%%",
             q->symbol, z_raw, y_raw, price_source,
             q->volume, is_zero_volume ? 1 : 0,
             has_open ? 1 : 0, has_high ? 1 : 0, has_low ? 1 : 0,
             has_ask_price ? 1 : 0, has_bid_price ? 1 : 0,
             market_closed_snapshot ? 1 : 0,
             q->is_valid ? 1 : 0, q->is_market_closed ? 1 : 0,
             q->current_price, q->change_percent);
}

esp_err_t twse_client_fetch(const char symbols[][8], uint8_t count,
                             stock_quote_t *results)
{
    if (!symbols || !results || count == 0 || count > MAX_STOCK_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Heap-allocate url+ex_ch to keep stack small (called from HTTPD 4KB stack) */
    char *ex_ch = calloc(1, 256);
    char *url = malloc(512);
    if (!ex_ch || !url) {
        free(ex_ch);
        free(url);
        return ESP_ERR_NO_MEM;
    }

    /* 組合 ex_ch 參數：tse_2330.tw|tse_2317.tw|... */
    for (int i = 0; i < count; i++) {
        if (i > 0) strlcat(ex_ch, "|", 256);
        char tmp[20];
        snprintf(tmp, sizeof(tmp), "tse_%s.tw", symbols[i]);
        strlcat(ex_ch, tmp, 256);
    }

    snprintf(url, 512,
             "%s?ex_ch=%s&json=1&delay=0", TWSE_BASE_URL, ex_ch);
    free(ex_ch);

    char *buf = malloc(HTTP_BUF_INIT_SIZE);
    if (!buf) {
        free(url);
        return ESP_ERR_NO_MEM;
    }

    http_ctx_t ctx = {
        .buf = buf,
        .capacity = HTTP_BUF_INIT_SIZE,
        .data_len = 0,
        .overflow = false,
    };

    esp_http_client_config_t cfg = {
        .url            = url,
        .event_handler  = http_event_handler,
        .user_data      = &ctx,
        .timeout_ms     = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(url);

    if (ret != ESP_OK || ctx.overflow) {
        ESP_LOGE(TAG, "HTTP 請求失敗: err=%s status=%d len=%u overflow=%d",
                 esp_err_to_name(ret),
                 status_code,
                 (unsigned)ctx.data_len,
                 ctx.overflow ? 1 : 0);
        free(ctx.buf);
        return ESP_FAIL;
    }

    ctx.buf[ctx.data_len] = '\0';
    ESP_LOGD(TAG, "TWSE 回應: %.200s...", ctx.buf);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(ctx.buf);
    free(ctx.buf);

    if (!root) {
        ESP_LOGE(TAG, "JSON 解析失敗");
        return ESP_FAIL;
    }

    cJSON *msg_array = cJSON_GetObjectItem(root, "msgArray");
    if (!cJSON_IsArray(msg_array)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    for (int i = 0; i < count; i++) {
        memset(&results[i], 0, sizeof(results[i]));
        strlcpy(results[i].symbol, symbols[i], sizeof(results[i].symbol));
    }

    bool matched[MAX_STOCK_COUNT] = {false};
    int n = cJSON_GetArraySize(msg_array);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(msg_array, i);
        cJSON *sym = cJSON_GetObjectItem(item, "c");
        cJSON *ch = cJSON_GetObjectItem(item, "ch");
        const char *raw_c = (sym && cJSON_IsString(sym) && sym->valuestring) ? sym->valuestring : NULL;
        const char *raw_ch = (ch && cJSON_IsString(ch) && ch->valuestring) ? ch->valuestring : NULL;
        const char *sym_from_ch = extract_symbol_from_ch(raw_ch);
        const char *sym_str = NULL;
        const char *fallback_source = "none";
        if (raw_c && raw_c[0] != '\0') {
            sym_str = raw_c;
        } else if (sym_from_ch && sym_from_ch[0] != '\0') {
            sym_str = sym_from_ch;
            fallback_source = "ch";
        } else if (i < count) {
            /* 某些回傳可能缺 c，退回請求順序對位 */
            sym_str = symbols[i];
            fallback_source = "index";
        } else {
            ESP_LOGW(TAG, "map i=%d raw_c=<null> raw_ch=%s -> skip(no_symbol)",
                     i, raw_ch ? raw_ch : "<null>");
            continue;
        }

        int target_idx = -1;
        for (int j = 0; j < count; j++) {
            if (strcmp(symbols[j], sym_str) == 0) {
                target_idx = j;
                break;
            }
        }
        if (target_idx < 0 && i < count) {
            /* 若 c 異常但順序仍一致，最後退回索引對位 */
            target_idx = i;
            sym_str = symbols[i];
            fallback_source = "index_last";
        }
        if (target_idx < 0 || target_idx >= count) {
            ESP_LOGW(TAG, "map i=%d raw_c=%s raw_ch=%s sym=%s fallback=%s -> skip(unmatched)",
                     i,
                     raw_c ? raw_c : "<null>",
                     raw_ch ? raw_ch : "<null>",
                     sym_str ? sym_str : "<null>",
                     fallback_source);
            continue;
        }
        ESP_LOGD(TAG, "map i=%d raw_c=%s raw_ch=%s sym=%s fallback=%s -> idx=%d req=%s",
                 i,
                 raw_c ? raw_c : "<null>",
                 raw_ch ? raw_ch : "<null>",
                 sym_str ? sym_str : "<null>",
                 fallback_source,
                 target_idx,
                 symbols[target_idx]);

        parse_stock_item(item, sym_str, &results[target_idx]);
        matched[target_idx] = true;

        if (results[target_idx].is_valid) {
            ESP_LOGI(TAG, "%s %s %.2f (%.2f%%)",
                     results[target_idx].symbol, results[target_idx].name,
                     results[target_idx].current_price, results[target_idx].change_percent);
        } else if (results[target_idx].is_market_closed) {
            ESP_LOGI(TAG, "%s 休市，昨收 %.2f",
                     results[target_idx].symbol, results[target_idx].yesterday_close);
        }
    }

    for (int i = 0; i < count; i++) {
        if (!matched[i]) {
            ESP_LOGW(TAG, "TWSE 本輪未回傳 symbol=%s", symbols[i]);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t twse_client_validate_symbol_with_quote(const char *symbol,
                                                 stock_symbol_info_t *out,
                                                 stock_quote_t *out_quote)
{
    if (!symbol || !out) return ESP_ERR_INVALID_ARG;

    memset(out, 0, sizeof(*out));
    strlcpy(out->symbol, symbol, sizeof(out->symbol));
    if (out_quote) {
        memset(out_quote, 0, sizeof(*out_quote));
        strlcpy(out_quote->symbol, symbol, sizeof(out_quote->symbol));
    }

    /* Heap-allocate url to keep stack small (called from HTTPD 4KB stack) */
    char *url = malloc(512);
    if (!url) return ESP_ERR_NO_MEM;
    snprintf(url, 512,
             "%s?ex_ch=tse_%s.tw&json=1&delay=0", TWSE_BASE_URL, symbol);

    char *buf = malloc(HTTP_BUF_INIT_SIZE);
    if (!buf) {
        free(url);
        return ESP_ERR_NO_MEM;
    }

    http_ctx_t ctx = {
        .buf = buf,
        .capacity = HTTP_BUF_INIT_SIZE,
        .data_len = 0,
        .overflow = false,
    };

    esp_http_client_config_t cfg = {
        .url            = url,
        .event_handler  = http_event_handler,
        .user_data      = &ctx,
        .timeout_ms     = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(url);

    if (ret != ESP_OK || ctx.overflow) {
        ESP_LOGE(TAG, "驗證代號 HTTP 失敗: err=%s status=%d len=%u overflow=%d",
                 esp_err_to_name(ret),
                 status_code,
                 (unsigned)ctx.data_len,
                 ctx.overflow ? 1 : 0);
        free(ctx.buf);
        return ESP_FAIL;
    }

    ctx.buf[ctx.data_len] = '\0';
    cJSON *root = cJSON_Parse(ctx.buf);
    free(ctx.buf);

    if (!root) {
        ESP_LOGE(TAG, "驗證代號 JSON 解析失敗");
        return ESP_FAIL;
    }

    cJSON *msg_array = cJSON_GetObjectItem(root, "msgArray");
    if (!cJSON_IsArray(msg_array) || cJSON_GetArraySize(msg_array) <= 0) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON *item = cJSON_GetArrayItem(msg_array, 0);
    if (!cJSON_IsObject(item)) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON *sym = cJSON_GetObjectItem(item, "c");
    cJSON *ex = cJSON_GetObjectItem(item, "ex");

    if (sym && cJSON_IsString(sym) &&
        strcmp(sym->valuestring, symbol) == 0) {
        out->exists = true;
    }

    parse_symbol_metadata(item, out);
    if (out_quote && out->exists) {
        parse_stock_item(item, symbol, out_quote);
    }

    if (ex && cJSON_IsString(ex)) {
        strlcpy(out->market, ex->valuestring, sizeof(out->market));
        if (strcasecmp(out->market, "tse") == 0) {
            strlcpy(out->market, "tse", sizeof(out->market));
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t twse_client_validate_symbol(const char *symbol, stock_symbol_info_t *out)
{
    return twse_client_validate_symbol_with_quote(symbol, out, NULL);
}

typedef struct {
    char    symbols[MAX_STOCK_COUNT][8];
    uint8_t count;
    uint32_t interval_s;
} fetch_task_args_t;

static void twse_fetch_task(void *arg)
{
    fetch_task_args_t *args = (fetch_task_args_t *)arg;
    stock_quote_t results[MAX_STOCK_COUNT];

    while (s_task_running) {
        memset(results, 0, sizeof(results));
        esp_err_t ret = twse_client_fetch(args->symbols, args->count, results);
        if (ret == ESP_OK && s_queue) {
            for (int i = 0; i < args->count; i++) {
                xQueueOverwrite(s_queue, &results[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(args->interval_s * 1000));
    }

    free(args);
    vTaskDelete(NULL);
}

esp_err_t twse_client_init(QueueHandle_t quote_queue)
{
    s_queue = quote_queue;
    ESP_LOGI(TAG, "TWSE Client 初始化完成");
    return ESP_OK;
}

esp_err_t twse_client_start_task(const char symbols[][8], uint8_t count,
                                  uint32_t interval_s)
{
    if (!symbols || count == 0 || count > MAX_STOCK_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_task_running) twse_client_stop_task();

    fetch_task_args_t *args = malloc(sizeof(fetch_task_args_t));
    if (!args) return ESP_ERR_NO_MEM;

    args->count = count;
    args->interval_s = interval_s;
    memcpy(args->symbols, symbols, count * 8);

    s_task_running = true;
    BaseType_t res = xTaskCreatePinnedToCore(twse_fetch_task, "twse_fetch",
                                              STACK_TWSE, args,
                                              TASK_PRIO_TWSE,
                                              &s_task_handle, 0);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

void twse_client_stop_task(void)
{
    s_task_running = false;
    if (s_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_task_handle = NULL;
    }
}
