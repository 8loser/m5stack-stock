#pragma once

#include "ui_compat.h"

typedef enum {
    LOG_TAG_STOCK = 0,
    LOG_TAG_WIFI,
    LOG_TAG_AI,
    LOG_TAG_SYS,
} log_tag_t;

typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

void screen_log_push(log_tag_t tag, log_level_t level, const char *msg);
lv_obj_t *screen_log_create(void);
void screen_log_refresh(void);
