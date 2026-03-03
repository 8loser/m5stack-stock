#include "scheduler.h"
#include "sleep_manager.h"
#include "app_event_bus.h"
#include "storage.h"
#include "twse_client.h"
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

/* Task notification bits */
#define NOTIFY_QUOTE_BIT        (1 << 0)
#define NOTIFY_FORCE_QUOTE_BIT  (1 << 1)

static schedule_config_t s_config;
static QueueHandle_t     s_quote_queue     = NULL;
static TimerHandle_t     s_quote_timer     = NULL;
static TaskHandle_t      s_scheduler_task  = NULL;
static bool              s_sntp_synced     = false;

/* 儲存的股票清單（供排程使用）*/
static stock_list_t      s_stock_list;

static void publish_scheduler_tick(bool force_fetch)
{
    app_event_t evt = {
        .type = APP_EVENT_SCHEDULER_TICK,
        .timestamp_ms = 0,
    };
    evt.data.scheduler.force_fetch = force_fetch;
    if (app_event_publish(&evt) != ESP_OK) {
        ESP_LOGW(TAG, "publish scheduler tick failed");
    }
}

static void enrich_quote_with_industry(stock_quote_t *quote)
{
    if (!quote || quote->symbol[0] == '\0') return;

    stock_meta_t meta = {0};
    if (storage_stock_meta_load(quote->symbol, &meta) == ESP_OK) {
        strlcpy(quote->industry, meta.industry, sizeof(quote->industry));
    } else {
        quote->industry[0] = '\0';
    }
}

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

static void do_fetch_quotes(bool force_fetch)
{
    if (!is_wifi_connected()) {
        ESP_LOGW(TAG, "WiFi 未連線，跳過報價抓取");
        return;
    }

    if (s_stock_list.count == 0) {
        ESP_LOGW(TAG, "股票清單為空，跳過報價抓取");
        return;
    }

    /* SNTP 未同步時跳過市場時段限制，避免 RTC 未校時誤判 */
    if (!force_fetch && s_sntp_synced && s_config.market_only && !rtc_bm8563_is_market_open()) {
        ESP_LOGI(TAG, "非市場時段，跳過報價抓取");
        return;
    }

    stock_quote_t quotes[MAX_STOCK_COUNT] = {0};

    esp_err_t ret = twse_client_fetch(
                        (const char(*)[8])s_stock_list.symbols,
                        s_stock_list.count,
                        quotes);
    if (ret == ESP_OK && s_quote_queue) {
        for (int i = 0; i < s_stock_list.count; i++) {
            if (quotes[i].symbol[0] == '\0') {
                ESP_LOGW(TAG, "跳過空 symbol 報價: idx=%d", i);
                continue;
            }
            if (!quotes[i].is_valid && !quotes[i].is_market_closed) {
                ESP_LOGW(TAG, "股票 %s 本輪無有效報價，保留前次顯示", quotes[i].symbol);
                continue;
            }
            enrich_quote_with_industry(&quotes[i]);
            xQueueSend(s_quote_queue, &quotes[i], 0);
        }
    }
}

static void quote_timer_cb(TimerHandle_t xTimer)
{
    if (s_scheduler_task) {
        xTaskNotify(s_scheduler_task, NOTIFY_QUOTE_BIT, eSetBits);
    }
}

static void scheduler_task(void *arg)
{
    uint32_t last_diag_ms = 0;

    /* 等待 WiFi 連線後啟動 SNTP */
    while (!is_wifi_connected()) {
        publish_scheduler_tick(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    init_sntp();

    /* 立即抓一次報價 */
    do_fetch_quotes(false);

    /* 主迴圈：等待 timer 通知 + 監控睡眠條件 */
    while (1) {
        uint32_t bits = 0;
        /* 最多等 1 秒；timer 到期會提前喚醒 */
        xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(1000));
        publish_scheduler_tick((bits & NOTIFY_FORCE_QUOTE_BIT) != 0);

        if (bits & NOTIFY_FORCE_QUOTE_BIT) {
            do_fetch_quotes(true);
        } else if (bits & NOTIFY_QUOTE_BIT) {
            do_fetch_quotes(false);
        }

        if (sleep_manager_is_enabled() && sleep_manager_should_sleep()) {
            ESP_LOGI(TAG, "進入休市睡眠模式");
            scheduler_stop();
            sleep_manager_enter(MARKET_OPEN_HOUR, MARKET_OPEN_MIN > 5
                                ? MARKET_OPEN_MIN - 5 : 0);
        }

        uint32_t now_ms = (uint32_t)esp_log_timestamp();
        if (now_ms - last_diag_ms >= 30000U) {
            UBaseType_t wm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "diag stack_hwm scheduler=%u", (unsigned)wm);
            last_diag_ms = now_ms;
        }
    }
}

esp_err_t scheduler_init(QueueHandle_t quote_queue)
{
    s_quote_queue     = quote_queue;

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

    if (!s_quote_timer) {
        ESP_LOGE(TAG, "Timer 建立失敗");
        return ESP_FAIL;
    }

    xTimerStart(s_quote_timer, 0);

    /* 排程監控任務 */
    BaseType_t res = xTaskCreatePinnedToCore(scheduler_task, "scheduler",
                                              STACK_SCHEDULER, NULL,
                                              TASK_PRIO_SCHEDULER,
                                              &s_scheduler_task, 1);

    ESP_LOGI(TAG, "Scheduler 啟動：報價間隔=%ds",
             s_config.quote_interval_s);
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
    ESP_LOGI(TAG, "排程設定已更新：報價=%ds market_only=%d",
             cfg->quote_interval_s, cfg->market_only ? 1 : 0);
    return ESP_OK;
}

void scheduler_get_config(schedule_config_t *cfg)
{
    *cfg = s_config;
}

void scheduler_trigger_quote_now(void)
{
    if (s_scheduler_task) {
        xTaskNotify(s_scheduler_task, NOTIFY_FORCE_QUOTE_BIT, eSetBits);
    }
}

uint32_t scheduler_get_seconds_to_next_quote(void)
{
    if (!s_quote_timer) {
        return 0;
    }

    TickType_t expiry_tick = xTimerGetExpiryTime(s_quote_timer);
    TickType_t now_tick = xTaskGetTickCount();
    TickType_t diff_tick = (expiry_tick > now_tick) ? (expiry_tick - now_tick) : 0;

    return (uint32_t)(diff_tick / configTICK_RATE_HZ);
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
    twse_client_stop_task();
    ESP_LOGI(TAG, "所有排程已停止");
}
