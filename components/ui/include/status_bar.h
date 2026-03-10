#pragma once

#include "ui_manager.h"
#include "ui_compat.h"
#include <stdbool.h>
#include <stdint.h>

void status_bar_create_on(lv_obj_t *parent);
void status_bar_set_visible(bool visible);
void status_bar_update_wifi(int state, const char *ip);
void status_bar_set_page(screen_id_t page);
void status_bar_set_dashboard_watch_count(uint8_t count);
