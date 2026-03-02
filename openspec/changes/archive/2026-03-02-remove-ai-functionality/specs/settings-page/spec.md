## MODIFIED Requirements

### Requirement: 點擊按鈕即時儲存
使用者點擊任一 interval 按鈕時，Settings 頁面 SHALL 立即：
1. 更新 `cfg.quote_interval_s`
2. 呼叫 `scheduler_apply_config(&cfg)` 使設定即時生效
3. 呼叫 `storage_schedule_save(&cfg)` 持久化到 NVS

#### Scenario: 點擊 5 min
- **WHEN** 使用者點擊 `5 min`
- **THEN** `quote_interval_s` 變為 300，並立即套用與儲存
