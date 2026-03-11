#include "alert_feedback.h"
#include "audio.h"
#include "vibration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} tone_step_t;

typedef struct {
    uint16_t vibration_ms;
    uint16_t tone_gap_ms;
    const tone_step_t *tones;
    size_t tone_count;
} feedback_pattern_t;

static const tone_step_t s_up_tones[] = {
    {880, 250},
    {1047, 250},
    {1319, 250},
    {1568, 250},
};

static const tone_step_t s_down_tones[] = {
    {1568, 250},
    {1319, 250},
    {1047, 250},
    {880, 250},
};

static const feedback_pattern_t s_pattern_up = {
    .vibration_ms = 1000,
    .tone_gap_ms = 0,
    .tones = s_up_tones,
    .tone_count = sizeof(s_up_tones) / sizeof(s_up_tones[0]),
};

static const feedback_pattern_t s_pattern_down = {
    .vibration_ms = 1000,
    .tone_gap_ms = 0,
    .tones = s_down_tones,
    .tone_count = sizeof(s_down_tones) / sizeof(s_down_tones[0]),
};

static const feedback_pattern_t *get_pattern(alert_feedback_type_t type)
{
    if (type == ALERT_FEEDBACK_DOWN) {
        return &s_pattern_down;
    }
    return &s_pattern_up;
}

void alert_feedback_play(alert_feedback_type_t type)
{
    const feedback_pattern_t *pattern = get_pattern(type);
    if (!pattern) {
        return;
    }

    vibration_pulse(pattern->vibration_ms);

    for (size_t i = 0; i < pattern->tone_count; i++) {
        audio_beep(pattern->tones[i].freq_hz, pattern->tones[i].duration_ms);
        if (i + 1 < pattern->tone_count && pattern->tone_gap_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(pattern->tone_gap_ms));
        }
    }
}
