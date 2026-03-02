#include "audio.h"
#include "axp192.h"
#include "app_config.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "audio";

#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define BUF_SAMPLES     256
#define TONE_AMPLITUDE  12000
#define FADE_MS         8

static i2s_chan_handle_t s_tx_chan = NULL;

esp_err_t audio_init(void)
{
    esp_err_t ret = axp192_set_speaker_enable(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "喇叭使能失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(SPEAKER_I2S_NUM,
                                                            I2S_ROLE_MASTER);
    ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel 建立失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                     I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = SPEAKER_BCK_GPIO,
            .ws   = SPEAKER_WS_GPIO,
            .dout = SPEAKER_DATA_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) return ret;

    ret = i2s_channel_enable(s_tx_chan);
    ESP_LOGI(TAG, "I2S 音效初始化完成");
    return ret;
}

void audio_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_tx_chan || freq_hz == 0) return;

    /* 每次播放前確保喇叭與 I2S TX 都在啟用狀態 */
    if (axp192_set_speaker_enable(true) != ESP_OK) {
        return;
    }
    esp_err_t ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return;
    }

    uint32_t total_samples = SAMPLE_RATE * duration_ms / 1000;
    int16_t *buf = (int16_t *)malloc(BUF_SAMPLES * 2 * sizeof(int16_t));
    if (!buf) return;

    uint32_t written_samples = 0;
    uint32_t sample_idx = 0;

    while (written_samples < total_samples) {
        uint32_t chunk = (total_samples - written_samples);
        if (chunk > BUF_SAMPLES) chunk = BUF_SAMPLES;

        for (uint32_t i = 0; i < chunk; i++) {
            uint32_t cur_idx = sample_idx++;
            float t = (float)cur_idx / SAMPLE_RATE;
            float gain = 1.0f;
            uint32_t fade_samples = (SAMPLE_RATE * FADE_MS) / 1000U;

            if (fade_samples > 0) {
                if (cur_idx < fade_samples) {
                    gain = (float)cur_idx / (float)fade_samples;
                } else if (total_samples > fade_samples &&
                           cur_idx > (total_samples - fade_samples)) {
                    gain = (float)(total_samples - cur_idx) / (float)fade_samples;
                }
            }
            if (gain < 0.0f) gain = 0.0f;

            int16_t sample = (int16_t)(TONE_AMPLITUDE * gain *
                                       sinf(2.0f * M_PI * freq_hz * t));
            buf[i * 2] = sample;
            buf[i * 2 + 1] = sample;
        }

        size_t bytes_written;
        i2s_channel_write(s_tx_chan, buf, chunk * 2 * sizeof(int16_t),
                          &bytes_written, pdMS_TO_TICKS(500));
        written_samples += chunk;
    }

    /* 靜音緩衝 */
    memset(buf, 0, BUF_SAMPLES * 2 * sizeof(int16_t));
    size_t bw;
    i2s_channel_write(s_tx_chan, buf, BUF_SAMPLES * 2 * sizeof(int16_t), &bw,
                      pdMS_TO_TICKS(100));
    free(buf);

    /* 停止時鐘與功放，避免殘留持續音/底噪 */
    ret = i2s_channel_disable(s_tx_chan);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "I2S disable 失敗: %s", esp_err_to_name(ret));
    }
    axp192_set_speaker_enable(false);
}

void audio_alert_up(void)
{
    audio_beep(880, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    audio_beep(1320, 150);
}

void audio_alert_down(void)
{
    audio_beep(880, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    audio_beep(587, 150);
}

void audio_notify(void)
{
    audio_beep(1047, 80);
    vTaskDelay(pdMS_TO_TICKS(30));
    audio_beep(1319, 80);
}

void audio_deinit(void)
{
    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
}
