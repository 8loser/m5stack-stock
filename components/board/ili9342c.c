#include "ili9342c.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ili9342c";

/* ILI9342C 初始化序列 */
static const uint8_t s_init_cmds[][20] = {
    /* cmd, len, data... */
    {0xCF, 3, 0x00, 0x83, 0x30},
    {0xED, 4, 0x64, 0x03, 0x12, 0x81},
    {0xE8, 3, 0x85, 0x01, 0x79},
    {0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02},
    {0xF7, 1, 0x20},
    {0xEA, 2, 0x00, 0x00},
    {0xC0, 1, 0x26},         /* 電源控制1 */
    {0xC1, 1, 0x11},         /* 電源控制2 */
    {0xC5, 2, 0x35, 0x3E},   /* VCOM */
    {0xC7, 1, 0xBE},
    {0x36, 1, 0x28},         /* 記憶體存取控制：橫向 */
    {0x3A, 1, 0x55},         /* 像素格式：RGB565 */
    {0xB1, 2, 0x00, 0x1B},   /* Frame rate */
    {0xF2, 1, 0x08},
    {0x26, 1, 0x01},         /* Gamma */
    {0xE0, 15, 0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00},
    {0xE1, 15, 0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F},
    {0x2A, 4, 0x00, 0x00, 0x01, 0x3F},  /* 列位址 */
    {0x2B, 4, 0x00, 0x00, 0x00, 0xEF},  /* 行位址 */
    {0x11, 0},   /* Sleep out */
    {0x29, 0},   /* Display on */
    {0x00, 0xFF} /* 結束標記 */
};

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;

static esp_err_t ili9342c_send_init_sequence(void)
{
    for (int i = 0; s_init_cmds[i][1] != 0xFF; ++i) {
        uint8_t cmd = s_init_cmds[i][0];
        uint8_t len = s_init_cmds[i][1];
        const void *data = (len > 0) ? &s_init_cmds[i][2] : NULL;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, cmd, data, len));

        /* Common panel delays: Sleep Out / Display On */
        if (cmd == 0x11) {
            vTaskDelay(pdMS_TO_TICKS(120));
        } else if (cmd == 0x29) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    return ESP_OK;
}

esp_err_t ili9342c_init(esp_lcd_panel_handle_t *panel_out,
                         esp_lcd_panel_io_handle_t *io_out)
{
    esp_err_t ret;

    /* SPI 匯流排設定 */
    spi_bus_config_t buscfg = {
        .mosi_io_num     = LCD_MOSI_GPIO,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_CLK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8,
    };
    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus 初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    /* LCD IO */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num        = LCD_DC_GPIO,
        .cs_gpio_num        = LCD_CS_GPIO,
        .pclk_hz            = 40 * 1000 * 1000,
        .lcd_cmd_bits       = 8,
        .lcd_param_bits     = 8,
        .spi_mode           = 0,
        .trans_queue_depth  = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io);
    if (ret != ESP_OK) return ret;

    /* Panel 驅動：IDF 5.1 內建 SPI RGB565 panel factory */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_GPIO,
        .color_space    = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel);
    if (ret != ESP_OK) return ret;

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    ret = ili9342c_send_init_sequence();
    if (ret != ESP_OK) return ret;
    esp_lcd_panel_mirror(s_panel, false, false);
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_disp_on_off(s_panel, true);

    *panel_out = s_panel;
    *io_out    = s_io;

    ESP_LOGI(TAG, "ILI9342C LCD 初始化完成");
    return ESP_OK;
}

esp_err_t ili9342c_register_flush_done_callback(esp_lcd_panel_io_color_trans_done_cb_t cb,
                                                void *user_ctx)
{
    if (s_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = cb,
    };
    return esp_lcd_panel_io_register_event_callbacks(s_io, &cbs, user_ctx);
}

esp_err_t ili9342c_flush(esp_lcd_panel_handle_t panel,
                          int x1, int y1, int x2, int y2,
                          const void *color_data)
{
    return esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, color_data);
}
