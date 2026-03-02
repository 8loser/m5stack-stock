## MODIFIED Requirements

### Requirement: scheduler_init 不接受 AI queue 參數
`scheduler_init()` SHALL 只接受 `quote_queue` 一個 queue 參數，不再接受 `ai_result_queue`。

#### Scenario: 初始化排程器
- **WHEN** `scheduler_init(g_quote_queue)` 被呼叫
- **THEN** 排程器正常啟動，只建立 quote timer，不建立 AI timer

## REMOVED Requirements

### Requirement: scheduler_trigger_ai_now
**Reason**: AI 功能暫時移除，不再需要手動觸發 AI 分析
**Migration**: 無，此函式無呼叫方
