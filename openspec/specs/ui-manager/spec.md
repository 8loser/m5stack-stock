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

