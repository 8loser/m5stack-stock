#pragma once
#include "twse_models.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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
 * @brief 啟動背景抓取任務
 */
esp_err_t twse_client_start_task(const char symbols[][8], uint8_t count,
                                  uint32_t interval_s);

/**
 * @brief 停止背景任務
 */
void twse_client_stop_task(void);
