#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

esp_err_t ili9342c_init(esp_lcd_panel_handle_t *panel_out,
                         esp_lcd_panel_io_handle_t *io_out);
esp_err_t ili9342c_register_flush_done_callback(esp_lcd_panel_io_color_trans_done_cb_t cb,
                                                 void *user_ctx);
esp_err_t ili9342c_flush(esp_lcd_panel_handle_t panel,
                          int x1, int y1, int x2, int y2,
                          const void *color_data);
