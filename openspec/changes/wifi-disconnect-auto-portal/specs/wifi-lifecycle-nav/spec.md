## ADDED Requirements

### Requirement: WiFi 斷線自動切換到 Portal 頁面
系統 SHALL 在 WiFi 狀態變為 `WIFI_STATE_FAILED`（retry 全部耗盡）時，自動將 UI 切換到 `SCREEN_PORTAL`。切換 SHALL 透過 main loop 消費 volatile flag 實現，不得從 event bus dispatch task 或 WiFi callback 直接呼叫 `ui_manager_switch_screen`。

#### Scenario: Runtime WiFi 斷線後自動切 portal
- **WHEN** WiFi 連線在正常運行中斷線，且 wifi_manager 5 次 retry 全部失敗（`WIFI_STATE_FAILED`）
- **THEN** UI 自動切換到 `SCREEN_PORTAL`，portal entry 邏輯啟動 AP portal、stop telegram bot、pause scheduler

#### Scenario: 已在 portal 頁面時收到 FAILED
- **WHEN** UI 目前已在 `SCREEN_PORTAL` 且收到 `WIFI_STATE_FAILED`
- **THEN** 不觸發任何頁面切換（no-op）

#### Scenario: Startup guard 期間不觸發自動導航
- **WHEN** `ui_manager_is_startup_guard_active()` 回傳 true 且收到 WiFi 狀態變化
- **THEN** 不設定 pending nav flag

### Requirement: WiFi 連線成功自動切回 Dashboard
系統 SHALL 在 WiFi 狀態變為 `WIFI_STATE_CONNECTED` 時，若 UI 目前在 `SCREEN_PORTAL`，自動切換回 `SCREEN_DASHBOARD`。切換前 SHALL 延遲 500ms，讓 portal HTTPD 的 in-flight HTTP response 完成。

#### Scenario: Portal 配網成功後自動切回 dashboard
- **WHEN** 使用者透過 portal 成功配網，WiFi 連線成功（`WIFI_STATE_CONNECTED`），且 UI 目前在 `SCREEN_PORTAL`
- **THEN** 延遲 500ms 後 UI 自動切換到 `SCREEN_DASHBOARD`，portal exit 邏輯恢復 scheduler + telegram bot

#### Scenario: WiFi 連線成功但不在 portal 頁面
- **WHEN** WiFi 連線成功且 UI 不在 `SCREEN_PORTAL`（例如在 dashboard）
- **THEN** 不觸發任何頁面切換，僅呼叫 `scheduler_service_notify_wifi_connected()`

#### Scenario: 延遲期間使用者手動切頁
- **WHEN** 500ms 延遲期間使用者手動從 portal 切到其他頁面
- **THEN** main loop 消費 pending nav 時 re-check current screen，若已非 portal 則跳過切換

### Requirement: 自動導航透過 main loop volatile flag 實現
`on_wifi_state()` callback SHALL 透過設定 `static volatile int s_pending_nav` 通知 main loop，main loop 每輪檢查並消費此 flag 執行 `ui_manager_switch_screen()`。此設計 SHALL 確保頁面切換在 app_main task context（8KB stack）執行。

#### Scenario: Main loop 消費 pending nav
- **WHEN** `s_pending_nav >= 0`
- **THEN** main loop 讀取目標 screen、重設 flag 為 -1、執行 `ui_manager_switch_screen(target)`

#### Scenario: WiFi flapping（快速連斷）
- **WHEN** WiFi 狀態在短時間內多次變化
- **THEN** `s_pending_nav` 以 last-write-wins 保留最新目標，main loop 只執行最終目標的切換
