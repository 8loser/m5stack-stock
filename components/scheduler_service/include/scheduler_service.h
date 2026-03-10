#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "storage.h"
#include "ai_provider.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief AI test key 單一 provider 結果
 */
typedef struct {
    bool     configured;
    bool     ok;
    int      status_code;
    esp_err_t err;
} sched_ai_test_provider_result_t;

/**
 * @brief AI test key 完整結果（由呼叫端提供，scheduler task 填入）
 */
typedef struct {
    int passed;
    int failed;
    int skipped;
    sched_ai_test_provider_result_t gemini;
    sched_ai_test_provider_result_t claude;
    sched_ai_test_provider_result_t openai;
} sched_ai_test_result_t;

/**
 * @brief 初始化排程器（含 SNTP 時間同步）
 */
esp_err_t scheduler_service_init(QueueHandle_t quote_queue);

/**
 * @brief 套用新的排程設定（可在執行期動態更新）
 */
esp_err_t scheduler_service_apply_config(const schedule_config_t *cfg);

/**
 * @brief 取得目前排程設定
 */
void      scheduler_service_get_config(schedule_config_t *cfg);

/**
 * @brief 手動觸發立即抓取報價
 */
void      scheduler_service_trigger_quote_now(void);

/**
 * @brief 取得距離下次報價觸發的剩餘秒數
 */
uint32_t  scheduler_service_get_seconds_to_next_quote(void);

/**
 * @brief 重新載入儲存的股票清單（下個排程週期生效）
 */
esp_err_t scheduler_service_reload_stock_list(void);

/**
 * @brief 停止所有排程（休市睡眠前呼叫）
 */
void      scheduler_service_stop(void);

/**
 * @brief 暫停週期報價抓取（保留 scheduler task）
 */
esp_err_t scheduler_service_pause_quote_polling(void);

/**
 * @brief 恢復週期報價抓取
 */
esp_err_t scheduler_service_resume_quote_polling(void);

/**
 * @brief 目前是否有進行中的報價抓取（含 HTTP）
 */
bool scheduler_service_is_quote_fetch_in_flight(void);

/**
 * @brief 等待進行中的報價抓取完成
 * @param timeout_ms 最多等待毫秒數
 */
esp_err_t scheduler_service_wait_quote_fetch_idle(uint32_t timeout_ms);

/**
 * @brief 重新載入 AtTime entries（API 儲存後呼叫）
 */
esp_err_t scheduler_service_reload_at_time(void);

/**
 * @brief 提交 AI test key 命令到 scheduler task 執行（同步等待結果）
 *
 * @param selected_provider -1=all, 0=Gemini, 1=Claude, 2=OpenAI
 * @param provided_api_key  使用者提供的 key（NULL 表示用 NVS 已存的）
 * @param result            呼叫端提供的結果 struct
 * @param timeout_ms        最多等待毫秒數
 * @return ESP_OK / ESP_ERR_TIMEOUT / ESP_FAIL
 */
esp_err_t scheduler_service_cmd_test_ai_key(int selected_provider,
                                             const char *provided_api_key,
                                             sched_ai_test_result_t *result,
                                             uint32_t timeout_ms);

/**
 * @brief 處理一筆最新報價以檢查漲跌門檻觸發（非阻塞）
 */
void scheduler_service_process_alert_quote(const stock_quote_t *quote);

/**
 * @brief 重新載入每檔股票 alert config（更新後立即生效）
 */
esp_err_t scheduler_service_reload_stock_alert_configs(void);

/**
 * @brief 取得 AtTime 固定全域 prompt 陣列（唯讀）
 * @param out_prompts 輸出陣列指標（可為 NULL）
 * @return 陣列元素數量
 */
size_t scheduler_service_get_at_time_fixed_global_prompts(const char *const **out_prompts);
