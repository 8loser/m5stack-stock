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
#define MAX_SYMBOLS     10

static QueueHandle_t s_queue        = NULL;
static TaskHandle_t  s_task_handle  = NULL;
static bool          s_task_running = false;

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
    strncpy(q->symbol, symbol, 7);
    q->is_valid = false;
    q->is_market_closed = false;

    cJSON *name = cJSON_GetObjectItem(item, "n");
    if (name && cJSON_IsString(name)) {
        strncpy(q->name, name->valuestring, 31);
    }

    /* 現價 z */
    cJSON *price = cJSON_GetObjectItem(item, "z");
    if (!price || !cJSON_IsString(price) ||
        strcmp(price->valuestring, "-") == 0) {
        /* 休市 */
        q->is_market_closed = true;
        /* 嘗試讀昨收 */
        cJSON *yday = cJSON_GetObjectItem(item, "y");
        if (yday && cJSON_IsString(yday) &&
            strcmp(yday->valuestring, "-") != 0) {
            q->current_price = strtof(yday->valuestring, NULL);
            q->yesterday_close = q->current_price;
        }
        return;
    }

    q->current_price = strtof(price->valuestring, NULL);

#define PARSE_FLOAT(key, field) do { \
    cJSON *_j = cJSON_GetObjectItem(item, key); \
    if (_j && cJSON_IsString(_j) && strcmp(_j->valuestring, "-") != 0) \
        q->field = strtof(_j->valuestring, NULL); \
} while(0)

    PARSE_FLOAT("o", open_price);
    PARSE_FLOAT("h", high_price);
    PARSE_FLOAT("l", low_price);
    PARSE_FLOAT("y", yesterday_close);

    cJSON *vol = cJSON_GetObjectItem(item, "v");
    if (vol && cJSON_IsString(vol) && strcmp(vol->valuestring, "-") != 0) {
        q->volume = strtol(vol->valuestring, NULL, 10);
    }

    cJSON *t = cJSON_GetObjectItem(item, "t");
    if (t && cJSON_IsString(t)) {
        strncpy(q->trade_time, t->valuestring, 15);
    }

    q->change_amount  = q->current_price - q->yesterday_close;
    q->change_percent = (q->yesterday_close > 0.0f)
                        ? (q->change_amount / q->yesterday_close * 100.0f)
                        : 0.0f;
    q->is_valid = true;
}

esp_err_t twse_client_fetch(const char symbols[][8], uint8_t count,
                             stock_quote_t *results)
{
    /* 組合 ex_ch 參數：tse_2330.tw|tse_2317.tw|... */
    char ex_ch[256] = {0};
    for (int i = 0; i < count; i++) {
        if (i > 0) strlcat(ex_ch, "|", sizeof(ex_ch));
        char tmp[20];
        snprintf(tmp, sizeof(tmp), "tse_%s.tw", symbols[i]);
        strlcat(ex_ch, tmp, sizeof(ex_ch));
    }

    char url[512];
    snprintf(url, sizeof(url),
             "%s?ex_ch=%s&json=1&delay=0", TWSE_BASE_URL, ex_ch);

    char *buf = malloc(HTTP_BUF_INIT_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

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

    int n = cJSON_GetArraySize(msg_array);
    for (int i = 0; i < n && i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(msg_array, i);
        cJSON *sym = cJSON_GetObjectItem(item, "c");
        const char *sym_str = (sym && cJSON_IsString(sym))
                              ? sym->valuestring : symbols[i];
        parse_stock_item(item, sym_str, &results[i]);

        if (results[i].is_valid) {
            ESP_LOGI(TAG, "%s %s %.2f (%.2f%%)",
                     results[i].symbol, results[i].name,
                     results[i].current_price, results[i].change_percent);
        } else if (results[i].is_market_closed) {
            ESP_LOGI(TAG, "%s 休市，昨收 %.2f",
                     results[i].symbol, results[i].yesterday_close);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t twse_client_validate_symbol(const char *symbol, stock_symbol_info_t *out)
{
    if (!symbol || !out) return ESP_ERR_INVALID_ARG;

    memset(out, 0, sizeof(*out));
    strlcpy(out->symbol, symbol, sizeof(out->symbol));

    char url[512];
    snprintf(url, sizeof(url),
             "%s?ex_ch=tse_%s.tw&json=1&delay=0", TWSE_BASE_URL, symbol);

    char *buf = malloc(HTTP_BUF_INIT_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

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

    if (ex && cJSON_IsString(ex)) {
        strlcpy(out->market, ex->valuestring, sizeof(out->market));
        if (strcasecmp(out->market, "tse") == 0) {
            strlcpy(out->market, "tse", sizeof(out->market));
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

typedef struct {
    char    symbols[MAX_SYMBOLS][8];
    uint8_t count;
    uint32_t interval_s;
} fetch_task_args_t;

static void twse_fetch_task(void *arg)
{
    fetch_task_args_t *args = (fetch_task_args_t *)arg;
    stock_quote_t results[MAX_SYMBOLS];

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
