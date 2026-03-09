#pragma once
#include "twse_models.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    char symbol[8];
    char name[64];
    char short_name[32];
    char industry[32];
    char market[8];
    bool exists;
} stock_symbol_info_t;

/**
 * @brief 初始化 TWSE Client
 * @param quote_queue 解析結果放入此 queue（stock_quote_t 元素）
 */
esp_err_t twse_client_init(QueueHandle_t quote_queue);

/**
 * @brief 立即抓取指定股票列表的報價
 * @param symbols  股票代號陣列（"tse_XXXX.tw" 格式）
 * @param count    股票數量
 * @param results  輸出陣列（需預先分配 count 個元素）
 */
esp_err_t twse_client_fetch(const char symbols[][8], uint8_t count,
                             stock_quote_t *results);

/**
 * @brief 驗證單一股票代號是否存在且回傳基本資訊
 */
esp_err_t twse_client_validate_symbol(const char *symbol, stock_symbol_info_t *out);

/**
 * @brief 驗證單一股票代號並可選擇同時解析即時報價（同一次 HTTP 回應）
 * @param symbol     股票代號（4 碼）
 * @param out_info   輸出：代號存在性與基本資訊
 * @param out_quote  輸出：報價（可為 NULL 表示不需要）
 */
esp_err_t twse_client_validate_symbol_with_quote(const char *symbol,
                                                 stock_symbol_info_t *out_info,
                                                 stock_quote_t *out_quote);

/**
 * @brief 啟動背景抓取任務
 */
esp_err_t twse_client_start_task(const char symbols[][8], uint8_t count,
                                  uint32_t interval_s);

/**
 * @brief 停止背景任務
 */
void twse_client_stop_task(void);
