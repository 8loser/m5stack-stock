#pragma once

#include "ui_manager.h"
#include "ui_compat.h"

void status_bar_create_on(lv_obj_t *parent);
void status_bar_update_wifi(int state, const char *ip);
void status_bar_set_page(screen_id_t page);
