#include "app_event_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#define EVENT_QUEUE_LEN          32
#define MAX_SUBSCRIBERS_PER_TYPE 8
#define DISPATCH_TASK_STACK      4096
#define DISPATCH_TASK_PRIO       3

typedef struct {
    app_event_cb_t cb;
    void *ctx;
} subscriber_entry_t;

static const char *TAG = "app_event_bus";
static QueueHandle_t s_event_queue = NULL;
static SemaphoreHandle_t s_sub_lock = NULL;
static bool s_inited = false;
static subscriber_entry_t s_subscribers[APP_EVENT_COUNT][MAX_SUBSCRIBERS_PER_TYPE];

static void dispatch_task(void *arg)
{
    (void)arg;
    app_event_t evt;

    while (1) {
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (evt.type >= APP_EVENT_COUNT) {
            continue;
        }

        if (xSemaphoreTake(s_sub_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
            for (int i = 0; i < MAX_SUBSCRIBERS_PER_TYPE; i++) {
                app_event_cb_t cb = s_subscribers[evt.type][i].cb;
                void *ctx = s_subscribers[evt.type][i].ctx;
                if (cb) {
                    cb(&evt, ctx);
                }
            }
            xSemaphoreGive(s_sub_lock);
        }
    }
}

esp_err_t app_event_bus_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(app_event_t));
    s_sub_lock = xSemaphoreCreateMutex();
    if (!s_event_queue || !s_sub_lock) {
        ESP_LOGE(TAG, "init failed");
        return ESP_ERR_NO_MEM;
    }

    memset(s_subscribers, 0, sizeof(s_subscribers));

    BaseType_t ok = xTaskCreatePinnedToCore(dispatch_task,
                                            "app_evt_dispatch",
                                            DISPATCH_TASK_STACK,
                                            NULL,
                                            DISPATCH_TASK_PRIO,
                                            NULL,
                                            1);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "dispatch task create failed");
        return ESP_FAIL;
    }

    s_inited = true;
    ESP_LOGI(TAG, "initialized");
    return ESP_OK;
}

esp_err_t app_event_publish(const app_event_t *evt)
{
    if (!s_inited || !evt) {
        return ESP_ERR_INVALID_STATE;
    }

    app_event_t local_evt = *evt;
    if (local_evt.timestamp_ms == 0) {
        local_evt.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    }

    if (xQueueSend(s_event_queue, &local_evt, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_event_subscribe(app_event_type_t type, app_event_cb_t cb, void *ctx)
{
    if (!s_inited || type >= APP_EVENT_COUNT || !cb) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_sub_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < MAX_SUBSCRIBERS_PER_TYPE; i++) {
        if (s_subscribers[type][i].cb == NULL) {
            s_subscribers[type][i].cb = cb;
            s_subscribers[type][i].ctx = ctx;
            xSemaphoreGive(s_sub_lock);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_sub_lock);
    return ESP_ERR_NO_MEM;
}
