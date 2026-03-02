## ADDED Requirements

### Requirement: 主流程心跳 API
`ui_manager` SHALL 提供主流程心跳 API，供 `main` 與 `scheduler` 任務更新活性時間戳，並提供查詢目前主流程是否存活的介面。

API 包含：
- `void ui_manager_heartbeat_feed_main(void)`
- `void ui_manager_heartbeat_feed_scheduler(void)`
- `bool ui_manager_is_main_flow_alive(uint32_t *age_main_ms, uint32_t *age_sched_ms)`

#### Scenario: main 任務餵心跳
- **WHEN** `main` 主迴圈呼叫 `ui_manager_heartbeat_feed_main()`
- **THEN** `ui_manager` 更新 main 心跳時間戳為當前時間

#### Scenario: scheduler 任務餵心跳
- **WHEN** `scheduler_task` 呼叫 `ui_manager_heartbeat_feed_scheduler()`
- **THEN** `ui_manager` 更新 scheduler 心跳時間戳為當前時間

### Requirement: 主流程存活判定門檻
`ui_manager` SHALL 以雙來源心跳判定主流程存活：僅當 `main` 與 `scheduler` 的心跳 age 都小於等於各自門檻時，回傳 alive=true。

預設門檻：
- main 門檻 = 1500 ms
- scheduler 門檻 = 3000 ms

#### Scenario: 雙來源均在門檻內
- **WHEN** main 與 scheduler 心跳 age 都在門檻內
- **THEN** `ui_manager_is_main_flow_alive(...)` 回傳 true

#### Scenario: 任一來源超過門檻
- **WHEN** main 或 scheduler 任一心跳 age 超過門檻
- **THEN** `ui_manager_is_main_flow_alive(...)` 回傳 false

### Requirement: Status bar 顯示主流程心跳指標
status bar SHALL 在時間文字前方顯示主流程心跳指標，並依 `ui_manager_is_main_flow_alive(...)` 結果切換樣式：
- alive=true：顯示愛心 emoji，使用雙擊節奏閃爍
- alive=false：顯示 `!`，停止閃爍且以警示色呈現

#### Scenario: 主流程正常
- **WHEN** `ui_manager_is_main_flow_alive(...)` 回傳 true
- **THEN** status bar 心跳指標顯示愛心 emoji 並以雙擊節奏閃爍

#### Scenario: 主流程疑似卡住
- **WHEN** `ui_manager_is_main_flow_alive(...)` 回傳 false
- **THEN** status bar 心跳指標改顯示 `!`，並停止閃爍
