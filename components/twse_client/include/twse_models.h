#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char  symbol[8];           /* "2330" */
    char  name[32];            /* "台積電" */
    char  industry[32];        /* "半導體" */
    float current_price;       /* 現價 */
    float open_price;          /* 開盤 */
    float high_price;          /* 最高 */
    float low_price;           /* 最低 */
    float yesterday_close;     /* 昨收 */
    float change_amount;       /* 漲跌額 */
    float change_percent;      /* 漲跌幅 % */
    long  volume;              /* 成交量（張）*/
    char  trade_time[16];      /* "13:28:00" */
    bool  is_valid;            /* false = 休市或解析失敗 */
    bool  is_market_closed;    /* true = TWSE 回傳 "-" */
} stock_quote_t;
