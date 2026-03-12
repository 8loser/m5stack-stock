#include "ft6336u.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "ft6336u";
static i2c_master_dev_handle_t s_dev = NULL;

/* FT6336U 暫存器 */
#define FT_REG_TD_STATUS    0x02
#define FT_REG_P1_XH        0x03
#define FT_REG_P1_XL        0x04
#define FT_REG_P1_YH        0x05
#define FT_REG_P1_YL        0x06

static esp_err_t ft_read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100);
}

esp_err_t ft6336u_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) return ret;

    /* 讀取韌體 ID 確認通訊 */
    uint8_t id = 0;
    ret = ft_read_regs(0xA6, &id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FT6336U 通訊失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "FT6336U 初始化完成 (fw_id=0x%02X)", id);
    return ESP_OK;
}

esp_err_t ft6336u_read(touch_point_t *point)
{
    uint8_t buf[5];
    esp_err_t ret = ft_read_regs(FT_REG_TD_STATUS, buf, 5);
    if (ret != ESP_OK) return ret;

    uint8_t touch_count = buf[0] & 0x0F;
    point->pressed = (touch_count > 0);

    if (point->pressed) {
        /* M5Core2 FT6336U: x/y 需要做座標轉換 */
        uint16_t raw_x = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
        uint16_t raw_y = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];

        /* 旋轉映射：原點在左上角 */
        point->x = raw_x;
        point->y = raw_y;
    }
    return ESP_OK;
}

bool ft6336u_is_touched(void)
{
    touch_point_t p = {0};
    if (ft6336u_read(&p) != ESP_OK) {
        return false;
    }
    return p.pressed;
}
