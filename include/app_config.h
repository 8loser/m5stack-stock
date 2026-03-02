#pragma once

/* ============================================================
 * M5Stack Core2 台股監測 — 全域設定
 * ============================================================ */

/* --- 硬體 --- */
#define LCD_WIDTH           320
#define LCD_HEIGHT          240
#define LCD_HOST            SPI2_HOST
#define LCD_MOSI_GPIO       23
#define LCD_CLK_GPIO        18
#define LCD_CS_GPIO         5
#define LCD_DC_GPIO         15
#define LCD_RST_GPIO        -1    /* AXP192 控制 */

#define TOUCH_SDA_GPIO      21
#define TOUCH_SCL_GPIO      22
#define TOUCH_INT_GPIO      39
#define TOUCH_ADDR          0x38

#define AXP192_I2C_ADDR     0x34
#define BM8563_I2C_ADDR     0x51

#define I2C_PORT_NUM        I2C_NUM_0
#define I2C_FREQ_HZ         400000

#define SPEAKER_I2S_NUM     I2S_NUM_0
#define SPEAKER_BCK_GPIO    12
#define SPEAKER_WS_GPIO     0
#define SPEAKER_DATA_GPIO   2

/* --- LVGL --- */
#define LVGL_TICK_PERIOD_MS     5
#define LVGL_BUF_LINES          30
#define UI_NON_HOME_IDLE_RETURN_MS 10000  /* 非 dashboard/portal 觸控閒置返回（毫秒） */

/* --- FreeRTOS 任務優先級 --- */
#define TASK_PRIO_LVGL          5
#define TASK_PRIO_WIFI          5
#define TASK_PRIO_TWSE          4
#define TASK_PRIO_SCHEDULER     2

/* --- FreeRTOS Stack 大小（byte）--- */
#define STACK_LVGL              8192
#define STACK_WIFI              6144
#define STACK_TWSE              6144
#define STACK_SCHEDULER         8192

/* --- NVS Namespace --- */
#define NVS_NS_WIFI             "wifi_cfg"
#define NVS_NS_AI               "ai_cfg"
#define NVS_NS_STOCKS           "stocks"
#define NVS_NS_SCHEDULE         "schedule"

/* --- 股票 --- */
#define MAX_STOCK_COUNT         10
#define DEFAULT_STOCKS          {"2330", "2317", "2409", "6770", "1802", "2367"}

/* --- 排程預設值 --- */
#define DEFAULT_QUOTE_INTERVAL_S    60      /* 報價更新間隔（秒）*/
#define DEFAULT_MARKET_ONLY         false   /* 全天候更新 */
#define DASHBOARD_SLOT_ROTATION_MS  1500    /* Dashboard 固定格位輪巡節拍（毫秒）*/

/* --- WiFi 手機配網 Portal --- */
#define WIFI_PORTAL_AP_SSID         "Core2-Setup"
#define WIFI_PORTAL_AP_PASSWORD     "core2wifi"
#define WIFI_PORTAL_AP_CHANNEL      1
#define WIFI_PORTAL_MAX_STA         4
#define WIFI_PORTAL_URL             "http://192.168.4.1"
#define WIFI_MAX_AP_COUNT           5
#define WIFI_SSID_MAX_LEN           33
#define WIFI_PASS_MAX_LEN           65

/* --- 台灣股市時段 --- */
#define MARKET_OPEN_HOUR        9
#define MARKET_OPEN_MIN         0
#define MARKET_CLOSE_HOUR       13
#define MARKET_CLOSE_MIN        30

/* --- TWSE API --- */
#define TWSE_BASE_URL           "https://mis.twse.com.tw/stock/api/getStockInfo.jsp"
#define HTTP_TIMEOUT_MS         15000

/* --- 震動馬達 --- */
#define VIBRATION_HAPTIC_MS     30    /* 觸控 haptic 震動時間 */
#define VIBRATION_ALERT_MS      200   /* 警報震動時間 */

/* --- M5Core2 FT6336U 底部虛擬按鍵感應區域 --- */
#define TOUCH_BTN_Y_MIN         LCD_HEIGHT  /* y >= 240 才視為底部按鍵區域，避免攔截正常觸控 */
#define TOUCH_BTN_A_X_MAX       (LCD_WIDTH / 3)         /* 左 1/3 */
#define TOUCH_BTN_B_X_MAX       ((LCD_WIDTH * 2) / 3)   /* 中 1/3；其餘為右 1/3 */
