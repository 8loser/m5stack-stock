## MODIFIED Requirements

### Requirement: WiFi 狀態變化寫入 log
`on_wifi_state()` callback SHALL 在 WiFi 狀態改變時呼叫 `ui_manager_log_wifi()`，記錄連線狀態轉換。此外 SHALL 根據 WiFi 狀態設定自動導航 flag 和通知 scheduler。

#### Scenario: WiFi 連線成功
- **WHEN** `on_wifi_state(WIFI_STATE_CONNECTED, ip)` 被觸發
- **THEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_INFO, "Connected: %s (%s)", ssid, ip)`
- **AND** 呼叫 `scheduler_service_notify_wifi_connected()`
- **AND** 若 UI 在 `SCREEN_PORTAL` 且 startup guard 未啟用，設定 `s_pending_nav = SCREEN_DASHBOARD`

#### Scenario: WiFi 斷線
- **WHEN** `on_wifi_state(WIFI_STATE_DISCONNECTED, NULL)` 被觸發
- **THEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_WARN, "Disconnected")`

#### Scenario: WiFi 連線失敗（retry 耗盡）
- **WHEN** `on_wifi_state(WIFI_STATE_FAILED, NULL)` 被觸發
- **THEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_WARN, "Disconnected")`
- **AND** 若 UI 不在 `SCREEN_PORTAL` 且 startup guard 未啟用，設定 `s_pending_nav = SCREEN_PORTAL`

## ADDED Requirements

### Requirement: Main loop 消費 pending nav flag
`app_main()` 主迴圈 SHALL 在每輪檢查 `s_pending_nav` volatile flag，若值 >= 0 則消費並執行頁面切換。

#### Scenario: Pending nav 為 SCREEN_DASHBOARD
- **WHEN** `s_pending_nav == SCREEN_DASHBOARD`
- **THEN** main loop 先 `vTaskDelay(500ms)` 等待 HTTP response 完成，re-check current screen，若仍在 portal 則呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`

#### Scenario: Pending nav 為 SCREEN_PORTAL
- **WHEN** `s_pending_nav == SCREEN_PORTAL`
- **THEN** main loop 呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)`（portal entry 邏輯自動處理 bot stop、scheduler pause、AP 啟動）

#### Scenario: 無 pending nav
- **WHEN** `s_pending_nav == -1`
- **THEN** 主迴圈不執行任何頁面切換

### Requirement: Boot 時 telegram bot 延後啟動
`app_main()` SHALL 在 `network_portal_connect_saved()` 成功後才呼叫 `telegram_bot_start()`。WiFi 連線失敗時不啟動 bot。

#### Scenario: Boot 有 WiFi
- **WHEN** `network_portal_connect_saved()` 回傳 `ESP_OK`
- **THEN** 呼叫 `telegram_bot_start()` 啟動 bot task

#### Scenario: Boot 無 WiFi
- **WHEN** `network_portal_connect_saved()` 回傳非 `ESP_OK`
- **THEN** 不呼叫 `telegram_bot_start()`，UI 切換到 `SCREEN_PORTAL`
