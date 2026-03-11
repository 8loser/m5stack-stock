#pragma once

typedef enum {
    ALERT_FEEDBACK_UP = 0,
    ALERT_FEEDBACK_DOWN,
} alert_feedback_type_t;

void alert_feedback_play(alert_feedback_type_t type);
