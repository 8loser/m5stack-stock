## ADDED Requirements

### Requirement: SCREEN_SETTINGS enum 新增
`ui_manager.h` SHALL 在 `screen_id_t` enum 中新增 `SCREEN_SETTINGS = 4`，位於 `SCREEN_INFO = 3` 之後、`SCREEN_COUNT` 之前。

#### Scenario: enum 值正確
- **WHEN** 程式碼引用 `SCREEN_SETTINGS`
- **THEN** 其整數值為 4，`SCREEN_COUNT` 為 5

### Requirement: screen_settings_load 在 switch_screen 時呼叫
`ui_manager_switch_screen()` SHALL 在切換目標為 `SCREEN_SETTINGS` 時，於持有 `g_ui_mutex` 的情況下呼叫 `screen_settings_load()`，確保 roller 與 checkbox 顯示最新排程設定。

#### Scenario: 切換至 Settings 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`
- **THEN** 在 `lv_scr_load_anim` 前，`screen_settings_load()` 被呼叫一次

### Requirement: screen_settings_create 加入初始化
`ui_manager_init()` SHALL 呼叫 `screen_settings_create()` 建立 Settings 頁面物件，並存入 `s_screens[SCREEN_SETTINGS]`。

#### Scenario: 初始化後可切換
- **WHEN** `ui_manager_init()` 完成
- **THEN** `s_screens[SCREEN_SETTINGS]` 非 NULL，`ui_manager_switch_screen(SCREEN_SETTINGS)` 可正常執行
