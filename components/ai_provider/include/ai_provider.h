#pragma once
#include "app_config.h"
#include "twse_models.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
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
                          const char *prompt,
                          const char *api_key,
                          ai_analysis_result_t *result);
    esp_err_t (*test_key)(const char *api_key, int *out_status_code);
    const char *name;
} ai_provider_ops_t;

/**
 * @brief 取得目前生效的 API Key（本機 NVS）
 */
void      ai_provider_get_active_api_key(char *out, size_t out_size);
esp_err_t ai_provider_reload_local_config(void);

/* ======== Public API ======== */

esp_err_t ai_provider_init(QueueHandle_t result_queue);

esp_err_t ai_provider_set_type(ai_provider_type_t type);
ai_provider_type_t ai_provider_get_type(void);
const char *ai_provider_get_name(void);

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

/**
 * @brief 同步分析（帶股票資料與自訂 prompt）
 */
esp_err_t ai_provider_analyze_with_prompt_sync(const stock_quote_t *quote,
                                                const char *prompt,
                                                const char *api_key,
                                                ai_analysis_result_t *result);

/**
 * @brief 同步分析（純 prompt，不帶股票資料）
 */
esp_err_t ai_provider_analyze_prompt_sync(const char *prompt,
                                           const char *api_key,
                                           ai_analysis_result_t *result);

/**
 * @brief 測試指定 Provider 的 API Key 可用性
 */
esp_err_t ai_provider_test_key(ai_provider_type_t type,
                               const char *api_key,
                               int *out_status_code);
