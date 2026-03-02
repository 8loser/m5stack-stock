## Context

Dashboard 從啟動到顯示股票資料需要：WiFi 連線 → SNTP 同步 → 市場時段判斷 → TWSE API 抓取 → queue 推送 → UI 更新。目前的問題是在「市場時段判斷」這一步發生了兩個錯誤：(1) `market_only` 設定被硬編碼為永遠 true；(2) SNTP 同步前 RTC 時間不可信，導致判斷錯誤。

現有 scheduler spec 有一條需求需要**反轉**：

> 「排程器 SHALL 在非市場開市時間固定跳過報價抓取，不由 UI 參數切換。」

此次設計會將這條需求修改為「由 `market_only` 設定控制」。

## Goals / Non-Goals

**Goals:**
- Dashboard 在 WiFi 連線後能顯示股票資料，不論當前時間
- `market_only` 設定實際控制是否限制交易時段抓取
- 首次開機（RTC 未同步）時不因時間錯誤而封鎖資料抓取
- `±` 符號改用字型相容字元

**Non-Goals:**
- 新增 AI 分析專屬頁面（AI 結果沿用 log 頁面顯示）
- 修改 TWSE API 解析邏輯或資料格式
- 改變 sleep_manager 行為

## Decisions

### Decision 1：`market_only` 條件判斷修正

**選擇**：`do_fetch_quotes()` 改為 `if (s_config.market_only && !rtc_bm8563_is_market_open())`

**理由**：最小改動，直接讀取已載入的 `s_config.market_only`。不需要新增 API 或傳遞額外參數。`storage_schedule_load()` 在 `scheduler_init()` 時已填入 `s_config`，預設值由 `storage_schedule_load()` 回傳（未設定時預設 `market_only = false`，見下方 Decision 3）。

**排除**：讓 `scheduler_task` 傳參數給 `do_fetch_quotes()` — 過度複雜，無必要。

---

### Decision 2：SNTP 競爭條件處理

**選擇**：當 `s_sntp_synced == false` 時，跳過市場時段過濾，直接抓取

修改 `do_fetch_quotes()` 判斷順序：
```
if (!s_sntp_synced) → 繼續抓（時間不可信，不做限制）
if (s_config.market_only && !rtc_bm8563_is_market_open()) → 跳過
```

**理由**：`s_sntp_synced` 旗標在 `scheduler.c` 已存在但未使用。此方案無需加入 delay 或 blocking wait，對 UX 最友善。TWSE API 在非交易時間仍會回傳昨收價（`is_market_closed = true`），已有正確的顯示路徑。

**排除**：等待 SNTP 同步後才首次抓取 — 可能延遲 30 秒以上，UX 差。

---

### Decision 3：storage_schedule_load 預設值

確認 `storage_schedule_load()` 在首次使用時的預設值。需要確保 `market_only` 預設為 `false`（全天候抓取），讓使用者在「未設定」時也能立即看到資料。檢查 `storage.c` 實作後補充。

---

### Decision 4：`±` 符號替換

**選擇**：改用 `+0.00%` / `-0.00%` 格式的初始佔位符，或直接用空字串

具體：將 `lv_label_set_text(s_change_labels[i], "+/-");` 作為初始值（純 ASCII），`screen_dashboard_update()` 的 `%+.2f%%` 格式不受影響。

---

### Decision 5：AI 結果 UI 更新範圍

**選擇**：本次不新增 AI 專屬頁面。`main.c` 的 AI queue consumer 目前已透過 `ui_manager_log_ai()` 將結果寫入 Log 頁面，此行為足夠。

**理由**：AI 分析頁面是獨立功能，需要新 screen 設計，超出此 bugfix 範圍。Log 頁面可見即合理。

## Risks / Trade-offs

| 風險 | 緩解措施 |
|------|---------|
| `market_only = false` 時，非交易時間 API 仍會呼叫，增加網路用量 | 使用者主動選擇，預設行為合理；TWSE API 免費無限制 |
| SNTP 未同步時抓取，RTC 顯示時間可能錯誤 | `rtc_bm8563_get_time()` 仍會回傳 RTC 時間（可能不準），`Updated:` 顯示時間可能偏差；等 SNTP 同步後下次抓取時間會自動修正 |
| 修改 scheduler spec 中「固定跳過」需求 | 需同步更新 `openspec/specs/scheduler/spec.md` delta spec |

## Open Questions

- `storage_schedule_load()` 在 NVS 未初始化時，`market_only` 預設值是 `false` 還是 `true`？需驗證 `storage.c` 實作。若預設為 `true`，則需要在說明文件中告知使用者需手動關閉。
