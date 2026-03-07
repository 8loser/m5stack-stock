## CHANGED Requirements

### Requirement: Log wrapper API

`ui_manager` SHALL 提供 AtTime log wrapper，封裝 `screen_log_push()` 呼叫：

- `void ui_manager_log_at(log_level_t level, const char *fmt, ...)`

wrapper SHALL 以 `vsnprintf` 格式化後寫入 `LOG_TAG_AT`。

#### Scenario: 呼叫 AtTime wrapper
- **WHEN** 呼叫 `ui_manager_log_at(LOG_LEVEL_INFO, "AT#%u AI_OK", idx)`
- **THEN** `screen_log_push(LOG_TAG_AT, LOG_LEVEL_INFO, "AT#<idx> AI_OK")` 被呼叫
