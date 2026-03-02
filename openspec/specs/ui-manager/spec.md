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
`ui_manager_switch_screen()` 呼叫 `status_bar_set_page(id)` 後，status bar SHALL 於中央區域顯示與目前 screen 對應的正確名稱，涵蓋全部六個 screen。

名稱對照如下：

| screen_id_t | 顯示名稱 |
|---|---|
| SCREEN_DASHBOARD | "儀表板" |
| SCREEN_LOG | "日誌" |
| SCREEN_INFO | "資訊" |
| SCREEN_SETTINGS | "設定" |
| SCREEN_PORTAL | "Portal"（靜態，不附加連線狀態） |
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
- **THEN** status bar 中央顯示靜態文字 "Portal"，不隨 WiFi 連線狀態變化

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
screen_portal SHALL 使用繁體中文顯示使用者可見文字。`s_status_lbl`（原顯示「入口已啟動 - 掃描 QR 加入 AP」）已移除，不再列入文字規格。

| 元素 | 文字 |
|------|------|
| 入口未啟動提示 | "入口未啟動\n\n等待自動啟動,\n再掃描 QR。" |
| 加入 AP 標題 | "加入 AP:" |
| 密碼標題 | "密碼:" |
| 配網 IP 標題 | "配網 IP:" |
| 內網 IP 標題 | "內網 IP:" |

#### Scenario: Portal 未啟動時顯示提示
- **WHEN** Portal AP 未啟動
- **THEN** 左側區域顯示 "入口未啟動" 相關中文提示，無頂部狀態標籤

#### Scenario: Portal 啟動時不顯示冗余狀態文字
- **WHEN** Portal AP 已啟動
- **THEN** 頁面不顯示「入口已啟動 - 掃描 QR 加入 AP」文字，只顯示 QR code 與 AP 資訊

#### Scenario: 內網 IP 僅於 WiFi 已連線時顯示
- **WHEN** Portal 頁面顯示且裝置尚未連上內網 WiFi
- **THEN** 不顯示「內網 IP」標題與值
- **WHEN** Portal 頁面顯示且裝置已連上內網 WiFi
- **THEN** 顯示「內網 IP」標題與值，且位置在「配網 IP」區塊下方

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
status_bar SHALL 使用繁體中文顯示時段和狀態資訊，且中央標題僅在超出可視寬度時向左捲動；文字可完整顯示時不得滾動。Portal 頁面 status bar 使用靜態英文 "Portal"，不顯示動態連線狀態。

| 元素 | 文字 |
|------|------|
| 電量 | "電量 --%"（預設） |
| Portal 頁面 | "Portal"（靜態，不附加連線狀態） |

#### Scenario: Status bar 電量顯示中文
- **WHEN** 電量資訊尚未取得
- **THEN** 顯示 "電量 --%"

#### Scenario: Portal 頁面 status bar 靜態顯示
- **WHEN** 在 Portal 頁面，不論 WiFi 連線狀態
- **THEN** status bar 中央固定顯示 "Portal"

#### Scenario: 短標題不滾動
- **WHEN** status bar 中央標題可完整顯示於可視寬度內
- **THEN** 標題保持固定，不進行滾動動畫

#### Scenario: 長標題向左滾動
- **WHEN** status bar 中央標題超出可視寬度
- **THEN** 標題啟用向左滾動顯示

### Requirement: 主流程心跳 API
`ui_manager` SHALL 提供主流程心跳 API，供 `main` 與 `scheduler` 任務更新活性時間戳，並提供查詢目前主流程是否存活的介面。

API 包含：
- `void ui_manager_heartbeat_feed_main(void)`
- `void ui_manager_heartbeat_feed_scheduler(void)`
- `bool ui_manager_is_main_flow_alive(uint32_t *age_main_ms, uint32_t *age_sched_ms)`

#### Scenario: main 任務餵心跳
- **WHEN** `main` 主迴圈呼叫 `ui_manager_heartbeat_feed_main()`
- **THEN** `ui_manager` 更新 main 心跳時間戳為當前時間

#### Scenario: scheduler 任務餵心跳
- **WHEN** `scheduler_task` 呼叫 `ui_manager_heartbeat_feed_scheduler()`
- **THEN** `ui_manager` 更新 scheduler 心跳時間戳為當前時間

### Requirement: 主流程存活判定門檻
`ui_manager` SHALL 以雙來源心跳判定主流程存活：僅當 `main` 與 `scheduler` 的心跳 age 都小於等於各自門檻時，回傳 alive=true。

預設門檻：
- main 門檻 = 1500 ms
- scheduler 門檻 = 3000 ms

#### Scenario: 雙來源均在門檻內
- **WHEN** main 與 scheduler 心跳 age 都在門檻內
- **THEN** `ui_manager_is_main_flow_alive(...)` 回傳 true

#### Scenario: 任一來源超過門檻
- **WHEN** main 或 scheduler 任一心跳 age 超過門檻
- **THEN** `ui_manager_is_main_flow_alive(...)` 回傳 false

### Requirement: Status bar 顯示主流程心跳指標
status bar SHALL 在時間文字前方顯示主流程心跳指標，並依 `ui_manager_is_main_flow_alive(...)` 結果切換樣式：
- alive=true：顯示愛心 emoji，使用雙擊節奏閃爍
- alive=false：顯示 `!`，停止閃爍且以警示色呈現

#### Scenario: 主流程正常
- **WHEN** `ui_manager_is_main_flow_alive(...)` 回傳 true
- **THEN** status bar 心跳指標顯示愛心 emoji 並以雙擊節奏閃爍

#### Scenario: 主流程疑似卡住
- **WHEN** `ui_manager_is_main_flow_alive(...)` 回傳 false
- **THEN** status bar 心跳指標改顯示 `!`，並停止閃爍

### Requirement: 非首頁畫面閒置超時自動返回 Dashboard
`ui_manager` SHALL 在目前畫面為非 `SCREEN_DASHBOARD` 且非 `SCREEN_PORTAL` 時，監測觸控閒置時間；若連續 10 秒無觸控按下事件，系統 SHALL 自動切換至 `SCREEN_DASHBOARD`。

#### Scenario: 非首頁畫面閒置超時返回
- **WHEN** 使用者停留在 `SCREEN_INFO`（或 `SCREEN_LOG` / `SCREEN_SETTINGS` / `SCREEN_HW_TEST`）且 10 秒內沒有觸控按下
- **THEN** `ui_manager` 會呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`

#### Scenario: Dashboard 不觸發閒置返回
- **WHEN** 使用者停留在 `SCREEN_DASHBOARD` 超過 10 秒且無觸控
- **THEN** 系統 SHALL NOT 因閒置邏輯觸發切頁

#### Scenario: Portal 不觸發閒置返回
- **WHEN** 使用者停留在 `SCREEN_PORTAL` 超過 10 秒且無觸控
- **THEN** 系統 SHALL NOT 因閒置邏輯觸發切頁

### Requirement: 觸控按下事件重置閒置倒數
`ui_manager` SHALL 在任意觸控按下事件發生時重置閒置倒數；此條件包含一般觸控區與底部虛擬按鍵觸控區。

#### Scenario: 一般觸控重置倒數
- **WHEN** 使用者在非首頁畫面發生一般觸控按下
- **THEN** 閒置計時基準更新，10 秒倒數重新開始

#### Scenario: 底部虛擬按鍵觸控重置倒數
- **WHEN** 使用者在底部虛擬按鍵區發生觸控按下
- **THEN** 閒置計時基準更新，10 秒倒數重新開始

### Requirement: 熄屏期間暫停閒置計時
當螢幕處於關閉狀態時，閒置返回邏輯 SHALL 暫停計時與判定；僅在亮屏期間進行超時檢查。

#### Scenario: 熄屏期間不計時
- **WHEN** 使用者在非首頁畫面熄屏並維持超過 10 秒
- **THEN** 系統 SHALL NOT 在熄屏期間觸發自動返回

#### Scenario: 亮屏後恢復檢查
- **WHEN** 使用者重新亮屏且仍在非首頁畫面
- **THEN** 系統恢復閒置倒數檢查，不以熄屏期間累積時間直接判定超時

### Requirement: 亮屏後顯示 Dashboard
系統在螢幕關閉期間 SHALL 預先切換至 `SCREEN_DASHBOARD`，使螢幕由關閉轉為開啟時直接顯示主監看頁，且不先短暫顯示關閉前頁面。

#### Scenario: 非首頁熄屏再亮屏回 Dashboard
- **WHEN** 使用者在 `SCREEN_INFO`（或其他非 Dashboard 畫面）熄屏後再亮屏
- **THEN** 亮屏第一時間顯示 `SCREEN_DASHBOARD`，不先顯示先前頁面

#### Scenario: 已在 Dashboard 亮屏維持 Dashboard
- **WHEN** 使用者在 `SCREEN_DASHBOARD` 熄屏後再亮屏
- **THEN** 畫面維持在 `SCREEN_DASHBOARD`
