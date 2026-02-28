#pragma once
#include "twse_models.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include <stdint.h>

/* ======== 型別定義 ======== */

typedef enum {
    AI_PROVIDER_GEMINI  = 0,
    AI_PROVIDER_CLAUDE  = 1,
    AI_PROVIDER_OPENAI  = 2,
} ai_provider_type_t;

typedef enum {
    AI_SIGNAL_BUY     = 0,
    AI_SIGNAL_SELL    = 1,
    AI_SIGNAL_HOLD    = 2,
    AI_SIGNAL_UNKNOWN = 3,
} ai_signal_t;

typedef struct {
    char    symbol[8];
    char    name[32];
    float   current_price;
    float   change_percent;
    float   low_price;
    float   high_price;
    long    volume;
} stock_context_t;

typedef struct {
    ai_signal_t signal;
    int         confidence;           /* 0-100 */
    char        analysis[AI_ANALYSIS_MAX_LEN];  /* 繁體中文說明 */
    esp_err_t   error_code;
} ai_analysis_result_t;

/* vtable 模式 */
typedef struct {
    esp_err_t (*analyze)(const stock_context_t *ctx,
                          const char *prompt_template,
                          const char *api_key,
                          ai_analysis_result_t *result);
    const char *name;
} ai_provider_ops_t;

/* ======== 遠端 Prompt 設定 ======== */

/**
 * 遠端 Prompt 檔案格式（JSON）：
 * {
 *   "system_prompt": "你是台灣股市分析師...",
 *   "extra_stocks": ["2330", "2317"],      // 額外分析的股票（可選）
 *   "signal_sources": ["twse", "news"],   // 訊號來源（保留字段）
 *   "analysis_template": "..."            // 覆蓋預設 prompt 模板（可選）
 * }
 */
typedef struct {
    char system_prompt[512];
    char extra_stocks[10][8];
    uint8_t extra_stock_count;
    char signal_sources[5][32];
    uint8_t signal_source_count;
    char analysis_template[1024];  /* 覆蓋內建 prompt，空字串則用預設 */
    bool is_loaded;
} remote_prompt_config_t;

/* ======== 遠端 Token 設定 ======== */

/**
 * 遠端 Token 設定檔格式（JSON）：
 * {
 *   "provider": "gemini",     // 必填："gemini" | "claude" | "openai"
 *   "api_key": "your-key"     // 必填：對應 provider 的 API Key
 * }
 * 開機後自動抓取，僅在 RAM 中使用（不寫回 NVS）。
 * 若抓取失敗，退回使用本地儲存的設定。
 */

/**
 * @brief 從遠端 URL 抓取並套用 Token 設定（provider + api_key）
 *        結果儲存在 RAM session 變數，不覆蓋 NVS。
 * @param url  完整 URL（http/https）
 */
esp_err_t ai_provider_fetch_remote_token_config(const char *url);

/**
 * @brief 取得目前生效的 API Key（優先遠端，次選本地 NVS）
 */
void      ai_provider_get_active_api_key(char *out, size_t out_size);

/* ======== Public API ======== */

esp_err_t ai_provider_init(QueueHandle_t result_queue);

esp_err_t ai_provider_set_type(ai_provider_type_t type);
ai_provider_type_t ai_provider_get_type(void);
const char *ai_provider_get_name(void);

/**
 * @brief 從網路下載並解析遠端 Prompt 設定檔
 * @param url  完整 URL（http/https），空字串則清除設定
 */
esp_err_t ai_provider_fetch_remote_prompt(const char *url);
const remote_prompt_config_t *ai_provider_get_remote_prompt(void);

/**
 * @brief 觸發 AI 分析（非阻塞，結果送入 result_queue）
 */
esp_err_t ai_provider_analyze_async(const stock_quote_t *quote,
                                     const char *api_key);

/**
 * @brief 同步分析（阻塞至完成）
 */
esp_err_t ai_provider_analyze_sync(const stock_quote_t *quote,
                                    const char *api_key,
                                    ai_analysis_result_t *result);
