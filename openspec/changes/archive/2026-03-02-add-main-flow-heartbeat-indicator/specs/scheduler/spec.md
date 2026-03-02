## ADDED Requirements

### Requirement: scheduler_task 提供週期性活性心跳
`scheduler_task` SHALL 每輪主迴圈呼叫 `ui_manager_heartbeat_feed_scheduler()`，使 UI 可判斷排程器仍存活。

#### Scenario: scheduler 正常運行時持續餵心跳
- **WHEN** `scheduler_task` 持續執行主迴圈
- **THEN** 每輪都呼叫 `ui_manager_heartbeat_feed_scheduler()` 一次

### Requirement: scheduler_task 等待粒度上限為 1 秒
`scheduler_task` 在等待通知時 SHALL 使用不超過 1000ms 的 timeout，以避免心跳更新間隔過長造成假性故障判定。

#### Scenario: 無通知時仍定期醒來
- **WHEN** `scheduler_task` 在該秒內未收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務最晚於 1000ms 內醒來，完成一次心跳更新與睡眠條件檢查

#### Scenario: 有通知時立即處理
- **WHEN** `scheduler_task` 在 timeout 前收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務立即處理報價抓取，且該輪仍會更新心跳
