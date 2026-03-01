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
