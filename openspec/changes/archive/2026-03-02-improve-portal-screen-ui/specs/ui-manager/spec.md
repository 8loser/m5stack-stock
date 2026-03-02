## MODIFIED Requirements

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
