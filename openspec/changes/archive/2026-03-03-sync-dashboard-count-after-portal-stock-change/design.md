## Context

現況流程中，Portal `POST /stocks/add` 與 `POST /stocks/remove` 會先更新 NVS，再呼叫 `scheduler_reload_stock_list()`。但 dashboard 的可見卡片數由 `ui_manager_set_dashboard_card_count()` 控制，該函式目前只在開機載入 stocks 時被呼叫，缺少清單變更後的同步機制。

專案已有類似解耦模式：`device_server` 透過 `wifi_state_cb_t` 回呼通知 `main`，再由 `main` 橋接到 UI。此變更可沿用同一模式，避免 `device_server` 直接依賴 `ui_manager`。

## Goals / Non-Goals

**Goals:**
- 在 Portal 新增/刪除股票成功後，立即同步 dashboard card count。
- 維持模組解耦，避免 `device_server` 直接 include/呼叫 UI 模組。
- 失敗路徑不觸發同步，避免 UI 與持久化資料不一致。

**Non-Goals:**
- 不變更股票報價抓取時機（不加入 `scheduler_trigger_quote_now()`）。
- 不改變 `/stocks` API 回傳格式。
- 不重構 dashboard 渲染與輪巡邏輯。

## Decisions

1. 新增 `stock_list_changed_cb_t` callback 於 `device_server`。
- Rationale: 最小變更即可把「清單已成功保存」事件向上層發布，與現有 WiFi callback 模式一致。
- Alternative considered: 讓 `device_server` 直接呼叫 `ui_manager_set_dashboard_card_count()`。
  - Rejected: 會引入跨元件耦合、破壞分層，且增加循環依賴風險。

2. 在 add/remove 成功寫入 NVS 後觸發 callback，傳遞 `list.count`。
- Rationale: 以儲存成功作為單一真實來源，避免提早通知造成狀態偏差。
- Alternative considered: 在 `scheduler_reload_stock_list()` 成功後才通知。
  - Rejected: scheduler 重載與 UI count 同步不是同一責任，且 count 來源已由 `list` 明確可得。

3. 在 `main` 內橋接 callback 到 `ui_manager_set_dashboard_card_count()`。
- Rationale: `main` 是現有跨模組組裝點，能保持 `device_server` 與 `ui_manager` 的邊界。
- Alternative considered: 新增獨立 event bus。
  - Rejected: 對此單一事件過度設計。

## Risks / Trade-offs

- [Risk] callback 在 HTTP server 任務上下文觸發，若 UI mutex 長時間忙碌可能造成短暫延遲。  
  → Mitigation: 沿用既有 `ui_manager_set_dashboard_card_count()` 內建 timeout/log，先保守維持同步呼叫。

- [Risk] 後續若新增其他修改股票清單入口，可能忘記觸發 callback。  
  → Mitigation: 明確在 spec 與 tasks 記錄「凡成功更新 stocks 清單皆需通知」。

- [Trade-off] 不立即觸發抓價，可能短時間看到新卡片但資料為 placeholder。  
  → Mitigation: 保持排程語意一致，避免本次需求擴張；由既有排程週期補齊報價。
