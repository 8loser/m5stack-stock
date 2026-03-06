#include "internal.h"
#include "rtc_bm8563.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>

static const char *TAG = "scheduler";

static void scheduler_service_sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    scheduler_service_ctx_t *ctx = scheduler_service_ctx();
    if (ctx) {
        ctx->sntp_synced = true;
    }
    ESP_LOGI(TAG, "SNTP 時間同步完成");

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

void scheduler_service_init_sntp(scheduler_service_ctx_t *ctx)
{
    (void)ctx;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.cloudflare.com");
    sntp_set_time_sync_notification_cb(scheduler_service_sntp_sync_cb);
    esp_sntp_init();

    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "SNTP 初始化完成（時區: CST+8）");
}
