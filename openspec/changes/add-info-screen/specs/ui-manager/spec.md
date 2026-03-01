## ADDED Requirements

### Requirement: screen_info_refresh 在 switch_screen 時呼叫
`ui_manager_switch_screen()` SHALL 在切換目標為 `SCREEN_INFO` 時，於持有 `g_ui_mutex` 的情況下呼叫 `screen_info_refresh()`，確保使用者看到最新資料。

#### Scenario: 切換至 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_INFO)`
- **THEN** 在 `lv_scr_load_anim` 前，`screen_info_refresh()` 被呼叫一次

#### Scenario: 切換至非 Info 頁面
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`
- **THEN** `screen_info_refresh()` 不被呼叫
