## REMOVED Requirements

### Requirement: AI Result Queue 消費並寫入 log
**Reason**: AI 功能暫時移除，`g_ai_result_queue` 和 `ui_manager_log_ai()` 一併移除
**Migration**: 無，主迴圈不再消費 AI queue；log screen 的 LOG_TAG_AI enum 值保留但不產生新條目
