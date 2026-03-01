## ADDED Requirements

### Requirement: WiFi 狀態變化寫入 log
`on_wifi_state()` callback SHALL 在 WiFi 狀態改變時呼叫 `ui_manager_log_wifi()`，記錄連線狀態轉換。

#### Scenario: WiFi 連線成功
- **WHEN** `on_wifi_state(WIFI_STATE_CONNECTED, ip)` 被觸發
- **THEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_INFO, "Connected: %s (%s)", ssid, ip)`（ssid 由 wifi_manager 取得）

#### Scenario: WiFi 斷線
- **WHEN** `on_wifi_state(WIFI_STATE_DISCONNECTED, NULL)` 被觸發
- **THEN** 呼叫 `ui_manager_log_wifi(LOG_LEVEL_WARN, "Disconnected")`

### Requirement: 系統啟動寫入 SYS log
`app_main()` SHALL 在初始化完成後寫入一筆 SYS log，標記系統啟動事件。

#### Scenario: 正常啟動
- **WHEN** 所有元件初始化成功，進入主迴圈前
- **THEN** 呼叫 `ui_manager_log_sys(LOG_LEVEL_INFO, "System ready")`

### Requirement: Quote 更新批次摘要寫入 log
主迴圈在每輪 drain `g_quote_queue` 後，若接收到至少 1 筆報價，SHALL 以單筆批次摘要形式寫入 STOCK log，而非逐筆寫入。

#### Scenario: 單輪接收到報價
- **WHEN** 主迴圈接收到 N 筆報價（N ≥ 1）
- **THEN** 呼叫 `ui_manager_log_stock(LOG_LEVEL_INFO, "Updated %d stock(s)", N)` 一次，而非 N 次

#### Scenario: 單輪無報價
- **WHEN** 主迴圈 `xQueueReceive(g_quote_queue)` 立即回傳 `pdFALSE`
- **THEN** 不寫入任何 STOCK log

### Requirement: AI Result Queue 消費並寫入 log
主迴圈 SHALL 非阻塞地輪詢 `g_ai_result_queue`，接收到 AI 分析結果時寫入 AI log。

#### Scenario: 收到 AI 分析結果
- **WHEN** `xQueueReceive(g_ai_result_queue, &ai_result, 0)` 回傳 `pdTRUE`
- **THEN** 呼叫 `ui_manager_log_ai(LOG_LEVEL_INFO, "AI: %.50s", ai_result.analysis)`（前 50 字元）

#### Scenario: 無 AI 結果
- **WHEN** `g_ai_result_queue` 為空
- **THEN** 主迴圈正常繼續，不阻塞
