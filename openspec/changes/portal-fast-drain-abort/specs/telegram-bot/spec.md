## CHANGED Requirements

### Requirement: telegram_bot_stop() 主動 abort in-flight HTTP

`telegram_bot_stop()` SHALL 設定停止 flag 並主動中斷正在進行的 HTTP 請求（abort 策略依 D2 決定）。中斷後 telegram_task SHALL 在 error path 正確 cleanup HTTP client 資源，無記憶體洩漏。

#### Scenario: 無 in-flight 請求時 stop
- **WHEN** telegram_task 處於 idle（非 HTTP 請求中）
- **THEN** 設 stop flag，task 在下個迴圈檢查後退出（行為不變）

#### Scenario: HTTP 請求進行中時 stop
- **WHEN** telegram_task 正在執行 `esp_http_client_perform()`
- **THEN** HTTP 請求被主動中斷，`perform()` 返回 error
- **AND** telegram_task cleanup HTTP client 後退出
- **AND** `telegram_bot_wait_stopped()` 在數百毫秒內返回 `ESP_OK`

#### Scenario: stop 後重新啟動
- **WHEN** 離開 Portal 後重新啟動 telegram bot
- **THEN** bot 正常運作，無殘留狀態影響
