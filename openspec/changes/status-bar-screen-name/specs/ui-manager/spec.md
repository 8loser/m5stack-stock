## ADDED Requirements

### Requirement: status bar 顯示所有 screen 的正確名稱
`ui_manager_switch_screen()` 呼叫 `status_bar_set_page(id)` 後，status bar SHALL 於中央區域顯示與目前 screen 對應的正確名稱，涵蓋全部五個 screen。

名稱對照如下：

| screen_id_t | 顯示名稱 |
|---|---|
| SCREEN_DASHBOARD | "Dashboard" |
| SCREEN_LOG | "Log" |
| SCREEN_INFO | "Info" |
| SCREEN_SETTINGS | "Settings" |
| SCREEN_PORTAL | （Portal 維持原有特殊格式） |

#### Scenario: 切換至 Settings 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`
- **THEN** status bar 中央顯示文字包含 "Settings"，不顯示 "Dashboard"

#### Scenario: 切換至 Dashboard 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`
- **THEN** status bar 中央顯示文字包含 "Dashboard"

#### Scenario: 切換至 Log 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_LOG)`
- **THEN** status bar 中央顯示文字包含 "Log"

#### Scenario: 切換至 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_INFO)`
- **THEN** status bar 中央顯示文字包含 "Info"

#### Scenario: 切換至 Portal 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)`
- **THEN** status bar 中央顯示文字包含 "Portal Setup"，維持原有格式
