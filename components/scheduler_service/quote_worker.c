#include "internal.h"
#include "app_config.h"
#include "rtc_bm8563.h"
#include "twse_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "scheduler";

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

bool scheduler_service_is_wifi_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

void scheduler_service_do_fetch_quotes(bool force_fetch)
{
    if (!scheduler_service_is_wifi_connected()) {
        ESP_LOGW(TAG, "WiFi 未連線，跳過報價抓取");
        scheduler_service_publish_quote_fetch_round(force_fetch,
                                                    APP_QUOTE_FETCH_REASON_WIFI_DISCONNECTED,
                                                    0, 0, 0, 0, ESP_OK);
        return;
    }

    if (s_stock_list.count == 0) {
        ESP_LOGW(TAG, "股票清單為空，跳過報價抓取");
        scheduler_service_publish_quote_fetch_round(force_fetch,
                                                    APP_QUOTE_FETCH_REASON_EMPTY_LIST,
                                                    0, 0, 0, 0, ESP_OK);
        return;
    }

    if (!force_fetch && s_sntp_synced && s_config.market_only && !rtc_bm8563_is_market_open()) {
        ESP_LOGI(TAG, "非市場時段，跳過報價抓取");
        scheduler_service_publish_quote_fetch_round(force_fetch,
                                                    APP_QUOTE_FETCH_REASON_MARKET_CLOSED,
                                                    s_stock_list.count, 0, 0, 0, ESP_OK);
        return;
    }

    stock_quote_t quotes[MAX_STOCK_COUNT] = {0};
    int total = s_stock_list.count;
    int pushed = 0;
    int skipped_empty_symbol = 0;
    int skipped_invalid = 0;

    s_quote_fetch_in_flight = true;
    esp_err_t ret = twse_client_fetch(
                        (const char(*)[8])s_stock_list.symbols,
                        s_stock_list.count,
                        quotes);
    s_quote_fetch_in_flight = false;

    if (ret == ESP_OK && s_quote_queue) {
        for (int i = 0; i < s_stock_list.count; i++) {
            if (quotes[i].symbol[0] == '\0') {
                ESP_LOGW(TAG, "跳過空 symbol 報價: idx=%d", i);
                skipped_empty_symbol++;
                continue;
            }
            if (!quotes[i].is_valid && !quotes[i].is_market_closed) {
                ESP_LOGW(TAG, "股票 %s 本輪無有效報價，保留前次顯示", quotes[i].symbol);
                skipped_invalid++;
                continue;
            }
            enrich_quote_with_industry(&quotes[i]);
            xQueueSend(s_quote_queue, &quotes[i], 0);
            pushed++;
        }
        ESP_LOGI(TAG, "quote_round total=%d pushed=%d skip_empty=%d skip_invalid=%d",
                 total, pushed, skipped_empty_symbol, skipped_invalid);
        scheduler_service_publish_quote_fetch_round(force_fetch,
                                                    APP_QUOTE_FETCH_REASON_FETCH_DONE,
                                                    s_stock_list.count,
                                                    (uint8_t)pushed,
                                                    (uint8_t)skipped_empty_symbol,
                                                    (uint8_t)skipped_invalid,
                                                    ESP_OK);
    } else {
        ESP_LOGW(TAG, "quote_round fetch_failed err=%s total=%d",
                 esp_err_to_name(ret), total);
        scheduler_service_publish_quote_fetch_round(force_fetch,
                                                    APP_QUOTE_FETCH_REASON_FETCH_FAILED,
                                                    s_stock_list.count, 0, 0, 0, ret);
    }
}
