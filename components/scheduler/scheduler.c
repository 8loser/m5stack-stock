#include "scheduler.h"
#include "sleep_manager.h"
#include "storage.h"
#include "twse_client.h"
#include "ai_provider.h"
#include "rtc_bm8563.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <time.h>

static const char *TAG = "scheduler";

static schedule_config_t s_config;
static QueueHandle_t     s_quote_queue     = NULL;
static QueueHandle_t     s_ai_result_queue = NULL;
static TimerHandle_t     s_quote_timer     = NULL;
static TimerHandle_t     s_ai_timer        = NULL;
static TaskHandle_t      s_scheduler_task  = NULL;
static bool              s_sntp_synced     = false;

/* 儲存的股票清單（供排程使用）*/
static stock_list_t      s_stock_list;
/* 最新報價（供 AI 分析使用）*/
static stock_quote_t     s_latest_quotes[MAX_STOCK_COUNT];
static int               s_latest_quote_count = 0;

static bool is_wifi_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

static void sntp_sync_cb(struct timeval *tv)
{
    s_sntp_synced = true;
    ESP_LOGI(TAG, "SNTP 時間同步完成");

    /* 同步到 RTC */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        rtc_time_t rtc_t = {
            .seconds = tm_info->tm_sec,
            .minutes = tm_info->tm_min,
            .hours   = tm_info->tm_hour,
            .day     = tm_info->tm_mday,
            .month   = tm_info->tm_mon + 1,
            .year    = tm_info->tm_year + 1900,
        };
        rtc_bm8563_set_time(&rtc_t);
        ESP_LOGI(TAG, "時間已同步至 RTC: %02d:%02d:%02d",
                 rtc_t.hours, rtc_t.minutes, rtc_t.seconds);
    }
}

static void init_sntp(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.cloudflare.com");
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();

    /* 設定台灣時區 */
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "SNTP 初始化完成（時區: CST+8）");
}

static void do_fetch_quotes(void)
{
    if (!is_wifi_connected()) {
        ESP_LOGW(TAG, "WiFi 未連線，跳過報價抓取");
        return;
    }

    if (s_stock_list.count == 0) {
        ESP_LOGW(TAG, "股票清單為空，跳過報價抓取");
        return;
    }

    /* 固定策略：僅在市場時段抓取報價 */
    if (!rtc_bm8563_is_market_open()) {
        ESP_LOGI(TAG, "非市場時段，跳過報價抓取");
        return;
    }

    memset(s_latest_quotes, 0, sizeof(s_latest_quotes));
    s_latest_quote_count = s_stock_list.count;

    esp_err_t ret = twse_client_fetch(
                        (const char(*)[8])s_stock_list.symbols,
                        s_stock_list.count,
                        s_latest_quotes);
    if (ret == ESP_OK && s_quote_queue) {
        for (int i = 0; i < s_stock_list.count; i++) {
            xQueueSend(s_quote_queue, &s_latest_quotes[i], 0);
        }
    }
}

static void do_ai_analysis(void)
{
    if (!is_wifi_connected()) {
        ESP_LOGW(TAG, "WiFi 未連線，跳過 AI 分析");
        return;
    }
    if (s_latest_quote_count == 0) {
        ESP_LOGW(TAG, "無報價資料，跳過 AI 分析");
        return;
    }

    /* 優先使用遠端 Token，次選本地 NVS Key */
    char api_key[128] = {0};
    ai_provider_get_active_api_key(api_key, sizeof(api_key));
    if (strlen(api_key) == 0) {
        ESP_LOGW(TAG, "未設定 API Key（本地或遠端），跳過 AI 分析");
        return;
    }

    /* 分析第一支股票（或最後更新的有效報價）*/
    for (int i = 0; i < s_latest_quote_count; i++) {
        if (s_latest_quotes[i].is_valid) {
            ESP_LOGI(TAG, "觸發 AI 分析: %s", s_latest_quotes[i].symbol);
            ai_provider_analyze_async(&s_latest_quotes[i], api_key);
            break;
        }
    }
}

static void quote_timer_cb(TimerHandle_t xTimer)
{
    do_fetch_quotes();
}

static void ai_timer_cb(TimerHandle_t xTimer)
{
    do_ai_analysis();
}

static void scheduler_task(void *arg)
{
    /* 等待 WiFi 連線後啟動 SNTP */
    while (!is_wifi_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    init_sntp();

    /* 立即抓一次報價 */
    do_fetch_quotes();

    /* 主迴圈：監控睡眠條件 */
    while (1) {
        if (sleep_manager_is_enabled() && sleep_manager_should_sleep()) {
            ESP_LOGI(TAG, "進入休市睡眠模式");
            scheduler_stop();
            /* 設定 08:55 喚醒 */
            sleep_manager_enter(MARKET_OPEN_HOUR, MARKET_OPEN_MIN > 5
                                ? MARKET_OPEN_MIN - 5 : 0);
            /* 不會繼續執行到這裡 */
        }
        vTaskDelay(pdMS_TO_TICKS(60000));  /* 每分鐘檢查一次 */
    }
}

esp_err_t scheduler_init(QueueHandle_t quote_queue,
                          QueueHandle_t ai_result_queue)
{
    s_quote_queue     = quote_queue;
    s_ai_result_queue = ai_result_queue;

    /* 載入設定 */
    storage_schedule_load(&s_config);
    storage_stocks_load(&s_stock_list);

    /* 初始化 sleep manager */
    sleep_manager_init();

    /* 報價 Timer */
    s_quote_timer = xTimerCreate("quote_tmr",
                                  pdMS_TO_TICKS(s_config.quote_interval_s * 1000),
                                  pdTRUE,
                                  NULL,
                                  quote_timer_cb);

    /* AI 分析 Timer（分鐘轉換為毫秒）*/
    s_ai_timer = xTimerCreate("ai_tmr",
                               pdMS_TO_TICKS((uint32_t)s_config.ai_interval_min * 60 * 1000),
                               pdTRUE,
                               NULL,
                               ai_timer_cb);

    if (!s_quote_timer || !s_ai_timer) {
        ESP_LOGE(TAG, "Timer 建立失敗");
        return ESP_FAIL;
    }

    xTimerStart(s_quote_timer, 0);
    xTimerStart(s_ai_timer, 0);

    /* 排程監控任務 */
    BaseType_t res = xTaskCreatePinnedToCore(scheduler_task, "scheduler",
                                              STACK_SCHEDULER, NULL,
                                              TASK_PRIO_SCHEDULER,
                                              &s_scheduler_task, 1);

    ESP_LOGI(TAG, "Scheduler 啟動：報價間隔=%ds AI分析間隔=%dmin",
             s_config.quote_interval_s, s_config.ai_interval_min);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}

esp_err_t scheduler_apply_config(const schedule_config_t *cfg)
{
    s_config = *cfg;
    storage_schedule_save(cfg);

    /* 更新 Timer 週期 */
    if (s_quote_timer) {
        xTimerChangePeriod(s_quote_timer,
                           pdMS_TO_TICKS(cfg->quote_interval_s * 1000), 0);
    }
    if (s_ai_timer) {
        xTimerChangePeriod(s_ai_timer,
                           pdMS_TO_TICKS((uint32_t)cfg->ai_interval_min * 60 * 1000), 0);
    }

    ESP_LOGI(TAG, "排程設定已更新：報價=%ds AI=%dmin",
             cfg->quote_interval_s, cfg->ai_interval_min);
    return ESP_OK;
}

void scheduler_get_config(schedule_config_t *cfg)
{
    *cfg = s_config;
}

void scheduler_trigger_quote_now(void)
{
    do_fetch_quotes();
}

void scheduler_trigger_ai_now(void)
{
    do_ai_analysis();
}

esp_err_t scheduler_reload_stock_list(void)
{
    esp_err_t ret = storage_stocks_load(&s_stock_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重載股票清單失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "股票清單已重載，count=%u（下個排程週期生效）", s_stock_list.count);
    return ESP_OK;
}

void scheduler_stop(void)
{
    if (s_quote_timer) xTimerStop(s_quote_timer, 0);
    if (s_ai_timer)    xTimerStop(s_ai_timer, 0);
    twse_client_stop_task();
    ESP_LOGI(TAG, "所有排程已停止");
}
