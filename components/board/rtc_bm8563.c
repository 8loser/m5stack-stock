#include "rtc_bm8563.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "bm8563";
static i2c_port_t s_port;
static uint8_t    s_addr;

/* BM8563 暫存器 */
#define BM_REG_CTRL1    0x00
#define BM_REG_CTRL2    0x01
#define BM_REG_SECONDS  0x02
#define BM_REG_ALARM_MIN 0x09

static uint8_t bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

static esp_err_t bm_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_port, s_addr, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t bm_read(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(s_port, s_addr, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(100));
}

esp_err_t rtc_bm8563_init(i2c_port_t port, uint8_t addr)
{
    s_port = port;
    s_addr = addr;

    /* 清除控制暫存器 */
    esp_err_t ret = bm_write(BM_REG_CTRL1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BM8563 初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    bm_write(BM_REG_CTRL2, 0x00);

    ESP_LOGI(TAG, "BM8563 RTC 初始化完成");
    return ESP_OK;
}

esp_err_t rtc_bm8563_get_time(rtc_time_t *t)
{
    uint8_t buf[7];
    esp_err_t ret = bm_read(BM_REG_SECONDS, buf, 7);
    if (ret != ESP_OK) return ret;

    t->seconds = bcd2dec(buf[0] & 0x7F);
    t->minutes = bcd2dec(buf[1] & 0x7F);
    t->hours   = bcd2dec(buf[2] & 0x3F);
    t->day     = bcd2dec(buf[3] & 0x3F);
    t->month   = bcd2dec(buf[5] & 0x1F);
    t->year    = bcd2dec(buf[6]) + 2000;

    return ESP_OK;
}

esp_err_t rtc_bm8563_set_time(const rtc_time_t *t)
{
    uint8_t buf[8];
    buf[0] = BM_REG_SECONDS;
    buf[1] = dec2bcd(t->seconds);
    buf[2] = dec2bcd(t->minutes);
    buf[3] = dec2bcd(t->hours);
    buf[4] = dec2bcd(t->day);
    buf[5] = 0x01;  /* weekday（不使用）*/
    buf[6] = dec2bcd(t->month);
    buf[7] = dec2bcd((uint8_t)(t->year - 2000));

    return i2c_master_write_to_device(s_port, s_addr, buf, 8, pdMS_TO_TICKS(100));
}

esp_err_t rtc_bm8563_set_alarm(const rtc_time_t *alarm)
{
    uint8_t buf[5];
    buf[0] = BM_REG_ALARM_MIN;
    buf[1] = dec2bcd(alarm->minutes);      /* 分鐘 */
    buf[2] = dec2bcd(alarm->hours);        /* 小時 */
    buf[3] = 0x80;                         /* 日期不使能 */
    buf[4] = 0x80;                         /* 星期不使能 */

    esp_err_t ret = i2c_master_write_to_device(s_port, s_addr, buf, 5,
                                               pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    /* 啟用 alarm interrupt */
    return bm_write(BM_REG_CTRL2, 0x02);
}

esp_err_t rtc_bm8563_clear_alarm(void)
{
    return bm_write(BM_REG_CTRL2, 0x00);
}

bool rtc_bm8563_is_market_open(void)
{
    rtc_time_t t;
    if (rtc_bm8563_get_time(&t) != ESP_OK) return false;

    /* 台灣股市：09:00 ~ 13:30 */
    uint16_t now  = t.hours * 60 + t.minutes;
    uint16_t open = MARKET_OPEN_HOUR  * 60 + MARKET_OPEN_MIN;
    uint16_t close = MARKET_CLOSE_HOUR * 60 + MARKET_CLOSE_MIN;

    return (now >= open && now < close);
}

time_t rtc_bm8563_to_unix(const rtc_time_t *t)
{
    struct tm tm_info = {
        .tm_sec  = t->seconds,
        .tm_min  = t->minutes,
        .tm_hour = t->hours,
        .tm_mday = t->day,
        .tm_mon  = t->month - 1,
        .tm_year = t->year - 1900,
    };
    return mktime(&tm_info);
}
