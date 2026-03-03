#pragma once

#include "twse_models.h"
#include "ui_compat.h"
#include <stdint.h>

lv_obj_t *screen_dashboard_create(void);
void screen_dashboard_set_card_count(uint8_t n);
void screen_dashboard_set_symbols(const char symbols[][8], uint8_t count);
void screen_dashboard_update(const stock_quote_t *q);
void screen_dashboard_refresh(void);
void screen_dashboard_on_enter(void);
void screen_dashboard_on_leave(void);
