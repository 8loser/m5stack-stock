#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "storage.h"

/**
 * @brief 初始化排程器（含 SNTP 時間同步）
 */
esp_err_t scheduler_init(QueueHandle_t quote_queue,
                          QueueHandle_t ai_result_queue);

/**
 * @brief 套用新的排程設定（可在執行期動態更新）
 */
esp_err_t scheduler_apply_config(const schedule_config_t *cfg);

/**
 * @brief 取得目前排程設定
 */
void      scheduler_get_config(schedule_config_t *cfg);

/**
 * @brief 手動觸發立即抓取報價
 */
void      scheduler_trigger_quote_now(void);

/**
 * @brief 手動觸發立即 AI 分析
 */
void      scheduler_trigger_ai_now(void);

/**
 * @brief 重新載入儲存的股票清單（下個排程週期生效）
 */
esp_err_t scheduler_reload_stock_list(void);

/**
 * @brief 停止所有排程（休市睡眠前呼叫）
 */
void      scheduler_stop(void);
