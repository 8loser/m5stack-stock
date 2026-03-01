# 計劃：小螢幕 UI 重構 — 底部按鍵導航 + Status Bar 修正

## Context

**根本問題**：`status_bar` 在 `lv_layer_top()`（y=0, h=20），永遠遮住所有子頁面最上方 20px，
Back 按鈕在 topbar 的 y=6 被完全遮蓋。WiFi 頁面的「WiFi Offline」標籤在設定頁毫無意義。

**目標**：
1. 加入底部三按鍵（BtnA/B/C）硬體導航：子頁面 Back、功能確認等
2. 子頁面移除螢幕內 Back 按鈕（BtnA 取代），釋放 header 空間
3. 子頁面 header 縮為 h=20（只顯示標題）
4. Status bar 只在 Dashboard 顯示
5. WiFi 頁面完整重設計：單個大 QR code（156x156）

---

## 底部按鍵邏輯規劃

```
[  BtnA: Back  ]  [  BtnB: Action  ]  [  BtnC: Next  ]
```

| Screen | BtnA | BtnB | BtnC |
|--------|------|------|------|
| DASHBOARD | （無動作） | 手動刷新股票 | 切到 Settings |
| AI_ANALYSIS | 返回 Dashboard | 觸發 AI 分析 | — |
| SCHEDULE | 返回 Dashboard | 儲存設定 | — |
| WIFI | 返回 Dashboard | 切換 Portal | — |
| SETTINGS | 返回 Dashboard | 儲存所有設定 | — |

---

## 修改範圍（共 8 個檔案）

| 檔案 | 改動性質 |
|------|---------|
| `include/app_config.h` | 新增 TOUCH_BTN 常數 |
| `components/ui/widgets/status_bar.c` | 新增 `status_bar_set_visible()` |
| `components/ui/ui_manager.c` | touch callback 攔截按鍵；`switch_screen` 控制 status bar；`handle_hw_button()` |
| `components/ui/screens/screen_wifi.c` | 移除 Back 按鈕，header h=40→20，大 QR，新增 `screen_wifi_on_btn()` |
| `components/ui/screens/screen_ai_analysis.c` | 移除 Back btn，header 縮減，新增 `screen_ai_analysis_on_btn()` |
| `components/ui/screens/screen_schedule.c` | 移除 Back btn，header 縮減，新增 `screen_schedule_on_btn()` |
| `components/ui/screens/screen_settings.c` | 移除 Back btn，header 縮減，新增 `screen_settings_on_btn()` |
| `components/ui/screens/screen_dashboard.c` | 新增 `screen_dashboard_on_btn()` for refresh |

---

## 步驟 1：app_config.h — 新增按鍵區域常數

```c
/* M5Core2 FT6336U 底部虛擬按鍵感應區域 */
#define TOUCH_BTN_Y_MIN     240   /* y >= 240 = 底部按鍵區域 */
#define TOUCH_BTN_A_X_MAX   110   /* x < 110 = BtnA (左) */
#define TOUCH_BTN_B_X_MAX   220   /* 110 <= x < 220 = BtnB (中) */
                                   /* x >= 220 = BtnC (右) */
```

---

## 步驟 2：status_bar.c — 新增 set_visible

在 `status_bar.c` 加入：
- `static lv_obj_t *s_bar = NULL;`（新增 static 變數）
- `status_bar_create_on()` 中 `s_bar = bar;`（儲存參照）
- 新函數 `void status_bar_set_visible(bool visible)` — hide/show `s_bar`

---

## 步驟 3：ui_manager.c — 核心修改

### 3a. `lvgl_touch_cb()` — 攔截底部按鍵

```c
static bool s_hw_btn_fired = false;   // 新增 static

// 在現有 lvgl_touch_cb 中，ft6336u_read(&pt) 之後：
if (pt.pressed && pt.y >= TOUCH_BTN_Y_MIN) {
    if (!s_hw_btn_fired) {
        s_hw_btn_fired = true;
        uint8_t btn = (pt.x < TOUCH_BTN_A_X_MAX) ? 0 :
                      (pt.x < TOUCH_BTN_B_X_MAX) ? 1 : 2;
        handle_hw_button(btn);
    }
    data->state = LV_INDEV_STATE_REL;  // 不傳給 LVGL
    return;
}
s_hw_btn_fired = false;
// 繼續原有的螢幕觸控邏輯...
```

### 3b. 新增 `handle_hw_button()` 函數

```c
static void handle_hw_button(uint8_t btn)  // 0=A, 1=B, 2=C
{
    extern void screen_dashboard_on_btn(uint8_t b);
    extern void screen_ai_analysis_on_btn(uint8_t b);
    extern void screen_schedule_on_btn(uint8_t b);
    extern void screen_wifi_on_btn(uint8_t b);
    extern void screen_settings_on_btn(uint8_t b);

    switch (s_cur_screen) {
        case SCREEN_DASHBOARD:
            if (btn == 1) screen_dashboard_on_btn(1);  // BtnB = refresh
            if (btn == 2) ui_manager_switch_screen(SCREEN_SETTINGS);  // BtnC = settings
            break;
        case SCREEN_AI_ANALYSIS:
            if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
            else screen_ai_analysis_on_btn(btn);
            break;
        case SCREEN_SCHEDULE:
            if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
            else screen_schedule_on_btn(btn);
            break;
        case SCREEN_WIFI:
            if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
            else screen_wifi_on_btn(btn);
            break;
        case SCREEN_SETTINGS:
            if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
            else screen_settings_on_btn(btn);
            break;
    }
}
```

### 3c. `ui_manager_switch_screen()` — 控制 status bar

```c
extern void status_bar_set_visible(bool v);
status_bar_set_visible(id == SCREEN_DASHBOARD);   // 新增這一行
```

---

## 步驟 4：screen_wifi.c — 完整重設計

**空間計算（status bar 隱藏 + header h=20 + 無 Back 按鈕）：**

| 元件 | x | y | w | h | 說明 |
|------|---|---|---|---|------|
| topbar | 0 | 0 | 320 | 20 | 只顯示標題，無按鈕 |
| title "WiFi Setup" | center | 0 | 320 | 20 | font=14, white |
| s_status_lbl | 4 | 22 | 312 | 14 | portal 狀態（font=10） |
| s_qr_area（active 時顯示）| 0 | 38 | 320 | 164 | QR + 右側資訊 |
| ├ QR code (單個) | 4 | 4 | 156 | 156 | WIFI: URI，白底黑碼 |
| ├ "Join AP:" | 166 | 4 | 150 | — | 標籤（0x64B5F6） |
| ├ ssid text | 166 | 18 | 150 | — | SSID（white） |
| ├ "Pwd:" | 166 | 38 | 150 | — | 標籤 |
| ├ password | 166 | 52 | 150 | — | 密碼（white） |
| ├ "URL:" | 166 | 78 | 150 | — | 標籤 |
| ├ "192.168.4.1" | 166 | 92 | 150 | — | URL（white） |
| └ "Auto-opens in browser" | 166 | 112 | 150 | — | 說明（0x888888, wrap） |
| s_btn_portal（保留） | 95 | 206 | 130 | 30 | Start/Stop Portal |

**移除**：`btn_back`（整個 Back 按鈕及其 callback）、`lv_font_montserrat_16` topbar title

**新增**：`void screen_wifi_on_btn(uint8_t btn)` — btn==1 執行現有 `btn_toggle_portal_cb` 的邏輯

**QR 策略**：只顯示 WIFI: URI（Step 1 加入 AP）。iOS/Android Captive Portal Detection 會自動開啟設定頁，不需要第二個 QR。

---

## 步驟 5：其他子頁面 — 統一 header 縮減

**通用改法**（screen_ai_analysis、screen_schedule、screen_settings）：

```c
// topbar: h=40 → h=20，移除 btn_back
lv_obj_set_size(topbar, LCD_WIDTH, 20);
// 刪除 btn_back 建立程式碼和 btn_back_cb
// title font: montserrat_16 → montserrat_14

// 各頁面內容 y 座標：
// 舊起始 y=44～50 → 新起始 y=24～28
// 整體上移約 20px
```

**各頁面新增 `on_btn()` 函數**（BtnB 呼叫）：
- `screen_ai_analysis_on_btn(btn==1)` — 觸發 AI 分析（同 btn_analyze_cb 邏輯）
- `screen_schedule_on_btn(btn==1)` — 儲存設定（同 btn_save_cb 邏輯）
- `screen_settings_on_btn(btn==1)` — 儲存所有設定（同 btn_save_cb 邏輯）

**screen_dashboard.c**：
- 新增 `screen_dashboard_on_btn(btn==1)` — 觸發手動刷新（呼叫 `scheduler_trigger_quote_now()`）

### screen_ai_analysis.c 特殊情況
btn_back 在 `s_screen` 直接建立（非在 topbar 內），bar 已是 h=20，需：
- 移除 btn_back（y=22, h=24）及 btn_back_cb
- 在 bar 內加入標題 "AI Analysis"
- provider_lbl 移至 bar 內（右側）
- stock_lbl 位置調整
- signal/conf/analysis_ta 整體上移 ~28px（y=50→22, y=76→48, y=102→74）
- analysis_ta 高度從 110 → 130
- btn_analyze 從 y=210 移除（BtnB 取代，但保留 on-screen 版位於 y=208）

### screen_settings.c flex 佈局說明
移除 topbar 的 btn_back 後，flex container 自動重排。
將 topbar h=40 → h=20，移除 btn_back，title 置中，font=14。

---

## 注意事項

1. **`ft6336u_read()` 不 clip y 座標**（raw_y 直接賦值），y>240 的觸控自然可讀取
2. **`s_hw_btn_fired` 防重複觸發**：手指按住底部不放只會觸發一次，放開後重置
3. **screen_ai_analysis.c 特殊情況**：btn_back 在 `s_screen` 上直接建立（非在 topbar 內）
4. **screen_settings.c 使用 flex**：移除 topbar 的 btn_back 後，flex container 自動重排

---

## 驗證方式

1. `./flash.sh --build-only` 確認編譯無誤
2. `./flash.sh` 燒錄後驗證：
   - Dashboard 顯示 status bar（時間、WiFi、電量）
   - 切換到任何子頁面：status bar 消失，header 只顯示標題（h=20），內容更多
   - 按底部 BtnA：返回 Dashboard，status bar 重新出現
   - WiFi 頁面：portal inactive 顯示 "Portal inactive" + Start Portal 按鈕；portal active 顯示 156x156 QR
   - WiFi 頁面按底部 BtnB：切換 portal 狀態
   - Dashboard 按底部 BtnB：觸發手動股票刷新
   - Dashboard 按底部 BtnC：切換到 Settings 頁面
   - 所有子頁面 BtnB：觸發該頁的主要動作（分析/儲存/切換portal）

---

## 實作狀態：已完成（2026-03-01）

### 實作備註
- `btn_analyze_cb` 已從 screen_ai_analysis.c 移除（on-screen 按鈕一併移除，BtnB 取代）
- `screen_schedule.c`、`screen_settings.c` 各抽出 `do_save()` / `do_save_all()` helper，供 on_btn 直接呼叫，避免 dummy lv_event_t 問題
- 編譯結果：zero warnings，zero errors，binary 1.5MB / 3MB（50% free）

---

# 計畫：WiFi Portal 新增 AP 掃描下拉選單

## Context
使用者希望 captive portal 設定頁面能讓 Core2 自動掃描附近 WiFi AP，並在網頁呈現下拉選單供選擇，而不必手動輸入 SSID。

目前狀態：
- `wifi_manager_scan()` 函式已完整實作（wifi_manager.c），可取得 SSID + RSSI
- Portal HTML 為硬編碼字串，只有純文字 input 欄位
- HTTP server 已定義 2 個 handler：`GET /`（頁面）、`POST /wifi`（送出）
- `max_uri_handlers = 8`，有空間新增 handler

## 修改方案

### 1. 新增 `GET /scan` JSON endpoint
在 `wifi_manager.c` 新增 handler，執行掃描並回傳 JSON：

```c
// 回傳格式：[{"ssid":"MyWifi","rssi":-65},{"ssid":"OtherAP","rssi":-80}]
static esp_err_t portal_scan_get_handler(httpd_req_t *req)
```

掃描邏輯：直接呼叫 `esp_wifi_scan_start / esp_wifi_scan_get_ap_records`。
SSID 字串在寫入 JSON 時進行 JSON escape（處理 `"` 和 `\`）。
輸出限制：最多 20 個 AP，JSON buffer 動態 malloc（2KB）。

### 2. 更新 Portal HTML 字串
將 SSID `<input>` 改為 `<select>`，加入 JavaScript 用安全的 DOM API（`new Option()` + `textContent`）填入選項，避免 XSS。

- 頁面載入即自動呼叫 `fetch('/scan')`
- 掃描期間 select 顯示 "Scanning..."
- 掃描完成後填入 AP 清單（含 RSSI dBm）
- 最後加入 "Other (manual)" 選項，展開手動輸入欄位
- 掃描失敗時自動切換為手動輸入模式

### 3. POST handler 調整
若 `ssid == "__manual__"` 或空，改讀 `ssid_manual` 欄位。

### 4. 修正 HTTP Header 長度限制
問題：`CONFIG_HTTPD_MAX_REQ_HDR_LEN` 預設 512 bytes，現代瀏覽器 POST 時帶的 headers 超過此限制，server 回傳 "header fields are too long" 錯誤。
修正：`sdkconfig.defaults` 加入 `CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024`。

## 修改檔案

| 檔案 | 修改內容 |
|------|---------|
| `components/wifi_manager/wifi_manager.c` | 新增 `json_escape()`、`portal_scan_get_handler()`、更新 HTML、POST handler 加邏輯、註冊 `/scan` URI |
| `sdkconfig.defaults` | 新增 `CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024` |
| `sdkconfig` | 同步更新 `CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024` |

## 注意事項
- 掃描是同步阻塞操作（約 1~2 秒），`/scan` 請求約延遲 1~2 秒回應
- JSON 中 SSID 需 escape（`"` → `\"`，`\` → `\\`）
- Portal 掃描時 WiFi 已在 APSTA 模式，可以掃描
- `httpd_config_t` 在 ESP-IDF v5.1.4 **沒有** `max_req_hdr_len` 欄位，只能靠 Kconfig（`sdkconfig`）設定
- `PORTAL_BODY_MAX_LEN` 從 256 → 512（容納 `ssid_manual` 欄位的 POST body）

## 實作狀態：已實作，待測試驗證

### 實作備註
- 新增 `#define PORTAL_SCAN_MAX_APS 20`
- `json_escape()` 接受 `const uint8_t *`（對應 `wifi_ap_record_t.ssid` 型別）
- ap_records 和 JSON buffer 皆從 heap 分配，handler 結束前釋放
- JS 使用 `new Option()` + `o.textContent`（非 `innerHTML`），防止 SSID 特殊字元造成 XSS
- 編譯結果：zero warnings，zero errors，binary 50% free
