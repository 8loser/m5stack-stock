#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "storage.h"

/**
 * @brief 初始化排程器（含 SNTP 時間同步）
 */
esp_err_t scheduler_init(QueueHandle_t quote_queue);

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
 * @brief 取得距離下次報價觸發的剩餘秒數
 */
uint32_t  scheduler_get_seconds_to_next_quote(void);

/**
 * @brief 重新載入儲存的股票清單（下個排程週期生效）
 */
esp_err_t scheduler_reload_stock_list(void);

/**
 * @brief 停止所有排程（休市睡眠前呼叫）
 */
void      scheduler_stop(void);

/**
 * @brief 暫停週期報價抓取（保留 scheduler task）
 */
esp_err_t scheduler_pause_quote_polling(void);

/**
 * @brief 恢復週期報價抓取
 */
esp_err_t scheduler_resume_quote_polling(void);
