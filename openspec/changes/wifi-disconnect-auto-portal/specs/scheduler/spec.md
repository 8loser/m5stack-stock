## MODIFIED Requirements

### Requirement: scheduler_service_task 等待粒度上限為 1 秒
`scheduler_service_task` 在等待通知時 SHALL 使用不超過 1000ms 的 timeout，以避免心跳更新間隔過長造成假性故障判定。

初始化階段（WiFi 尚未連線）SHALL 使用相同的 `xTaskNotifyWait` 機制等待 WiFi，同時處理 cmd queue 和心跳。不得使用阻塞式 busy-wait。

#### Scenario: 無通知時仍定期醒來
- **WHEN** `scheduler_service_task` 在該秒內未收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務最晚於 1000ms 內醒來，完成一次心跳更新與睡眠條件檢查

#### Scenario: 有通知時立即處理
- **WHEN** `scheduler_service_task` 在 timeout 前收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務立即處理報價抓取，且該輪仍會更新心跳

#### Scenario: WiFi 等待期間收到 cmd queue 通知
- **WHEN** scheduler task 在初始化段等待 WiFi 時收到 `NOTIFY_CMD_BIT`
- **THEN** 立即處理 cmd queue（如 AI test key），不因 WiFi 未連線而阻塞

#### Scenario: WiFi 連線通知喚醒 scheduler
- **WHEN** scheduler task 在初始化段等待 WiFi 時收到 `NOTIFY_WIFI_UP_BIT`
- **THEN** 立即跳出 WiFi 等待迴圈，進入 SNTP 初始化和正常運作

## ADDED Requirements

### Requirement: scheduler_service_notify_wifi_connected API
系統 SHALL 提供 `scheduler_service_notify_wifi_connected()` 函式，透過 `xTaskNotify` 送出 `SCHEDULER_SERVICE_NOTIFY_WIFI_UP_BIT` 通知 scheduler task。

#### Scenario: WiFi 連線後通知 scheduler
- **WHEN** WiFi 狀態變為 `WIFI_STATE_CONNECTED` 且 `on_wifi_state()` 被呼叫
- **THEN** 呼叫 `scheduler_service_notify_wifi_connected()`，scheduler task 收到通知後跳出 WiFi 等待迴圈

#### Scenario: Scheduler task 尚未建立時呼叫
- **WHEN** `scheduler_service_notify_wifi_connected()` 在 scheduler task 建立前被呼叫
- **THEN** 函式安全返回（guard check `s_ctx.scheduler_task != NULL`）

### Requirement: Scheduler timer 條件啟動
`scheduler_service_init()` SHALL 根據 WiFi 連線狀態決定是否啟動 quote timer：
- WiFi 已連線：`xTimerStart` 啟動 timer
- WiFi 未連線：設定 `quote_polling_paused = true`，timer 不啟動

#### Scenario: 有 WiFi 時 init 啟動 timer
- **WHEN** `scheduler_service_init()` 被呼叫且 WiFi 已連線
- **THEN** quote timer 啟動，`quote_polling_paused = false`

#### Scenario: 無 WiFi 時 init 不啟動 timer
- **WHEN** `scheduler_service_init()` 被呼叫且 WiFi 未連線
- **THEN** quote timer 不啟動，`quote_polling_paused = true`，等待 `scheduler_service_resume_quote_polling()` 恢復
