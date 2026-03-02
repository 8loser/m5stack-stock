# ui-manager Specification

## Purpose
TBD - created by archiving change add-log-screen. Update Purpose after archive.
## Requirements
### Requirement: screen_log_refresh 在 switch_screen 時呼叫
`ui_manager_switch_screen()` SHALL 在切換目標為 `SCREEN_LOG` 時，於持有 `g_ui_mutex` 的情況下呼叫 `screen_log_refresh()`，以確保使用者看到最新 log 內容。

#### Scenario: 切換至 Log 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_LOG)`
- **THEN** 在 lv_scr_load_anim 前，`screen_log_refresh()` 被呼叫一次

#### Scenario: 切換至非 Log 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`
- **THEN** `screen_log_refresh()` 不被呼叫

### Requirement: Log wrapper API
`ui_manager` SHALL 提供 4 個 thread-safe log wrapper，封裝 `screen_log_push()` 呼叫，供外部元件使用而不需 include `screen_log.h`：

- `void ui_manager_log_stock(log_level_t level, const char *fmt, ...)`
- `void ui_manager_log_wifi(log_level_t level, const char *fmt, ...)`
- `void ui_manager_log_ai(log_level_t level, const char *fmt, ...)`
- `void ui_manager_log_sys(log_level_t level, const char *fmt, ...)`

每個 wrapper SHALL 使用 `vsnprintf` 格式化訊息後呼叫 `screen_log_push()`。不需持有 `g_ui_mutex`（`screen_log_push()` 自帶 portMUX 保護）。

#### Scenario: 呼叫 log_wifi wrapper
- **WHEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_INFO, "Connected: %s", ip)`
- **THEN** `screen_log_push(LOG_TAG_WIFI, LOG_LEVEL_INFO, "Connected: <ip>")` 被呼叫，訊息正確格式化

#### Scenario: 格式化訊息截斷
- **WHEN** 格式化後訊息超過 55 字元
- **THEN** 截斷至 55 字元，不發生 overflow

### Requirement: screen_info_refresh 在 switch_screen 時呼叫
`ui_manager_switch_screen()` SHALL 在切換目標為 `SCREEN_INFO` 時，於持有 `g_ui_mutex` 的情況下呼叫 `screen_info_refresh()`，確保使用者看到最新資料。

#### Scenario: 切換至 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_INFO)`
- **THEN** 在 `lv_scr_load_anim` 前，`screen_info_refresh()` 被呼叫一次

#### Scenario: 切換至非 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`
- **THEN** `screen_info_refresh()` 不被呼叫

### Requirement: SCREEN_SETTINGS enum 新增
`ui_manager.h` SHALL 在 `screen_id_t` enum 中新增 `SCREEN_SETTINGS = 4`，位於 `SCREEN_INFO = 3` 之後、`SCREEN_COUNT` 之前。

#### Scenario: enum 值正確
- **WHEN** 程式碼引用 `SCREEN_SETTINGS`
- **THEN** 其整數值為 4，`SCREEN_COUNT` 為 5

### Requirement: screen_settings_load 在 switch_screen 時呼叫
`ui_manager_switch_screen()` SHALL 在切換目標為 `SCREEN_SETTINGS` 時，於持有 `g_ui_mutex` 的情況下呼叫 `screen_settings_load()`，確保 interval 按鈕狀態顯示最新排程設定。

#### Scenario: 切換至 Settings 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`
- **THEN** 在 `lv_scr_load_anim` 前，`screen_settings_load()` 被呼叫一次

### Requirement: screen_settings_create 加入初始化
`ui_manager_init()` SHALL 呼叫 `screen_settings_create()` 建立 Settings 頁面物件，並存入 `s_screens[SCREEN_SETTINGS]`。

#### Scenario: 初始化後可切換
- **WHEN** `ui_manager_init()` 完成
- **THEN** `s_screens[SCREEN_SETTINGS]` 非 NULL，`ui_manager_switch_screen(SCREEN_SETTINGS)` 可正常執行

### Requirement: SCREEN_SETTINGS 納入硬體按鍵輪詢導航
`ui_manager.c` 的 `s_nav_screens[]` SHALL 包含 `SCREEN_SETTINGS`，使 `btn=0/2` 頁面輪詢可切換至 Settings 頁面。

#### Scenario: 由硬體按鍵切換至 Settings
- **WHEN** 使用者使用 `btn=0` 或 `btn=2` 進行頁面輪詢
- **THEN** 輪詢序列包含 `SCREEN_SETTINGS`

### Requirement: status bar 顯示所有 screen 的正確名稱
`ui_manager_switch_screen()` 呼叫 `status_bar_set_page(id)` 後，status bar SHALL 於中央區域顯示與目前 screen 對應的正確繁體中文名稱，涵蓋全部六個 screen。

名稱對照如下：

| screen_id_t | 顯示名稱 |
|---|---|
| SCREEN_DASHBOARD | "儀表板" |
| SCREEN_LOG | "日誌" |
| SCREEN_INFO | "資訊" |
| SCREEN_SETTINGS | "設定" |
| SCREEN_PORTAL | "入口設定"（後接連線狀態） |
| SCREEN_HW_TEST | "硬體測試" |

#### Scenario: 切換至 Settings 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`
- **THEN** status bar 中央顯示文字包含 "設定"

#### Scenario: 切換至 Dashboard 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`
- **THEN** status bar 中央顯示文字包含 "儀表板"

#### Scenario: 切換至 Log 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_LOG)`
- **THEN** status bar 中央顯示文字包含 "日誌"

#### Scenario: 切換至 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_INFO)`
- **THEN** status bar 中央顯示文字包含 "資訊"

#### Scenario: 切換至 HW Test 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_HW_TEST)`
- **THEN** status bar 中央顯示文字包含 "硬體測試"

#### Scenario: 切換至 Portal 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)`
- **THEN** status bar 中央顯示文字包含 "入口設定"

### Requirement: SCREEN_HW_TEST enum 新增
`ui_manager.h` SHALL 在 `screen_id_t` enum 中新增 `SCREEN_HW_TEST`，位於現有最後一個頁面 enum 之後、`SCREEN_COUNT` 之前。

#### Scenario: enum 可正常引用
- **WHEN** 程式碼引用 `SCREEN_HW_TEST`
- **THEN** 其值為有效的 screen_id_t，`SCREEN_COUNT` 自動更新為正確總數

### Requirement: screen_hw_test_create 加入初始化
`ui_manager_init()` SHALL 呼叫 `screen_hw_test_create()` 建立 HW Test 頁面物件，並存入 `s_screens[SCREEN_HW_TEST]`。

#### Scenario: 初始化後可切換
- **WHEN** `ui_manager_init()` 完成
- **THEN** `s_screens[SCREEN_HW_TEST]` 非 NULL，`ui_manager_switch_screen(SCREEN_HW_TEST)` 可正常執行

### Requirement: SCREEN_HW_TEST 納入硬體按鍵輪詢導航
`ui_manager.c` 的 `s_nav_screens[]` SHALL 包含 `SCREEN_HW_TEST`，使 Core2 左右鍵輪詢可切換到 HW Test 頁面。

#### Scenario: 左右鍵輪詢包含 HW Test
- **WHEN** 使用者使用 btn=0 或 btn=2 進行頁面輪詢
- **THEN** 輪詢序列包含 `SCREEN_HW_TEST`

### Requirement: Dashboard 頁面繁體中文文字
screen_dashboard SHALL 使用繁體中文顯示所有使用者可見文字，字型為 Noto Sans TC 子集。

| 元素 | 中文文字 |
|------|---------|
| 休市狀態 | "休市" |
| 更新時間前綴 | "更新: " |
| 漲跌符號 | "+/-"（保留） |
| 預設值 | "---"、"---.--"（保留） |

#### Scenario: 休市時顯示中文
- **WHEN** `is_market_closed` 為 true
- **THEN** Dashboard 顯示 "休市"

#### Scenario: 更新時間顯示中文前綴
- **WHEN** 股價資料更新
- **THEN** 時間戳前綴為 "更新: "

### Requirement: Settings 頁面繁體中文文字
screen_settings SHALL 使用繁體中文顯示所有使用者可見文字，且標題 SHALL 使用較大字級並水平置中。

| 元素 | 中文文字 |
|------|---------|
| 報價間隔標題 | "報價間隔" |
| 時間選項 | "1 分鐘"、"5 分鐘"、"10 分鐘" |
| 儲存成功訊息 | "儲存成功" |
| 儲存失敗訊息 | "儲存失敗" |

#### Scenario: 設定頁顯示中文標題
- **WHEN** 切換至 Settings 頁面
- **THEN** 標題顯示 "報價間隔" 且置中顯示

#### Scenario: 儲存成功顯示中文
- **WHEN** 設定儲存成功
- **THEN** 顯示 "儲存成功"

### Requirement: Info 頁面繁體中文文字
screen_info SHALL 使用繁體中文顯示所有使用者可見文字。

| 元素 | 中文文字 |
|------|---------|
| 區段標題 | "裝置"、"網路"、"AI"、"股票" |
| 連線狀態 | "已連線"、"連線中"、"離線" |
| 未設定狀態 | "未儲存"、"未設定" |
| 裝置資訊 | "記憶體: %lu bytes\n晶片: ..." |
| 網路資訊 | "SSID: %s\n狀態: %s\nIP: %s" |
| AI 資訊 | "供應商: %s\nAPI Key: %s" |

#### Scenario: Info 頁面區段標題為中文
- **WHEN** 切換至 Info 頁面
- **THEN** 四個區段標題分別為 "裝置"、"網路"、"AI"、"股票"

#### Scenario: WiFi 已連線狀態為中文
- **WHEN** WiFi 已連線
- **THEN** 網路區段狀態顯示 "已連線"

### Requirement: Portal 頁面繁體中文文字
screen_portal SHALL 使用繁體中文顯示所有使用者可見文字。

| 元素 | 中文文字 |
|------|---------|
| 入口啟動 | "入口已啟動 - 掃描 QR 加入 AP" |
| 入口未啟動 | "入口未啟動\n\n等待自動啟動,\n再掃描 QR。" |
| 操作說明 | "1. 加入 AP\n2. 開啟瀏覽器\n3. 提交 WiFi" |
| 加入 AP | "加入 AP:" |
| 密碼 | "密碼:" |
| 網址 | "網址:" |

#### Scenario: Portal 啟動時顯示中文
- **WHEN** Portal AP 已啟動
- **THEN** 顯示 "入口已啟動 - 掃描 QR 加入 AP"

#### Scenario: Portal 未啟動時顯示中文
- **WHEN** Portal AP 未啟動
- **THEN** 顯示 "入口未啟動" 相關中文提示

### Requirement: Log 頁面繁體中文文字
screen_log SHALL 使用繁體中文顯示所有使用者可見文字。

| 元素 | 中文文字 |
|------|---------|
| 頁面標題 | "事件日誌" |
| 分類標籤 | "股票"、"WiFi"、"AI"、"系統" |

#### Scenario: Log 頁面標題為中文
- **WHEN** 切換至 Log 頁面
- **THEN** 標題顯示 "事件日誌"

#### Scenario: Log 分類標籤為中文
- **WHEN** Log 頁面顯示
- **THEN** 分類 tab 顯示 "股票"、"WiFi"、"AI"、"系統"

### Requirement: HW Test 頁面繁體中文文字
screen_hw_test SHALL 使用繁體中文顯示所有使用者可見文字。

| 元素 | 中文文字 |
|------|---------|
| 區段標題 | "震動測試"、"音效測試" |
| 按鈕文字 | "震動"、"嗶聲" |

#### Scenario: HW Test 頁面顯示中文
- **WHEN** 切換至 HW Test 頁面
- **THEN** 顯示 "震動測試"、"音效測試" 及對應中文按鈕文字

### Requirement: Status bar 繁體中文文字
status_bar SHALL 使用繁體中文顯示時段和狀態資訊，且中央標題僅在超出可視寬度時向左捲動；文字可完整顯示時不得滾動。

| 元素 | 中文文字 |
|------|---------|
| 電量 | "電量 --%"（預設）|
| Portal 已連線 | "入口設定 - 已連線 (%s)" |
| Portal 連線中 | "入口設定 - 連線中..." |
| Portal 離線 | "入口設定 - 離線" |

#### Scenario: Status bar 電量顯示中文
- **WHEN** 電量資訊尚未取得
- **THEN** 顯示 "電量 --%"

#### Scenario: Portal 狀態中文
- **WHEN** 在 Portal 頁面且 WiFi 已連線
- **THEN** status bar 顯示 "入口設定 - 已連線 (IP)"

#### Scenario: 短標題不滾動
- **WHEN** status bar 中央標題可完整顯示於可視寬度內
- **THEN** 標題保持固定，不進行滾動動畫

#### Scenario: 長標題向左滾動
- **WHEN** status bar 中央標題超出可視寬度
- **THEN** 標題啟用向左滾動顯示
