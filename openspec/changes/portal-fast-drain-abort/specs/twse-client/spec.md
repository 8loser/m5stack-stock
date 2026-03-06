## CHANGED Requirements

### Requirement: quote fetch 支援外部 abort

quote fetch（由 scheduler 驅動）SHALL 支援在 HTTP 請求進行中被外部 abort。`scheduler_pause_quote_polling()` 呼叫後，若有 in-flight fetch，SHALL 主動中斷 HTTP 請求。

#### Scenario: 無 in-flight fetch 時 pause
- **WHEN** fetch 處於 idle
- **THEN** 設 pause flag，下次不再發起 fetch（行為不變）

#### Scenario: fetch 進行中時 pause
- **WHEN** fetch 正在執行 `esp_http_client_perform()`
- **THEN** HTTP 請求被主動中斷，fetch 在 error path cleanup 後返回
- **AND** `scheduler_wait_quote_fetch_idle()` 在數百毫秒內返回 `ESP_OK`

#### Scenario: resume 後 fetch 正常
- **WHEN** 離開 Portal 後 resume quote polling
- **THEN** 下次排程正常發起 fetch，無殘留狀態影響
