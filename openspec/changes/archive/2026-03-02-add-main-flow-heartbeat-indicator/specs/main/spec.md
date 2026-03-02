## ADDED Requirements

### Requirement: app_main 主迴圈提供活性心跳
`app_main()` 主迴圈 SHALL 在每輪迴圈呼叫 `ui_manager_heartbeat_feed_main()`，供 status bar 判斷主流程存活狀態。

#### Scenario: 主迴圈正常運作
- **WHEN** `app_main` 進入 while 迴圈並完成該輪工作
- **THEN** 呼叫 `ui_manager_heartbeat_feed_main()` 一次

#### Scenario: 主迴圈阻塞超過門檻
- **WHEN** 主迴圈因阻塞未在門檻內再次呼叫 `ui_manager_heartbeat_feed_main()`
- **THEN** `ui_manager_is_main_flow_alive(...)` 可回報主流程非存活狀態
