## Why

Dashboard 在非交易時間永遠顯示佔位符（`---`），無法看到任何股票資料。根本原因是 scheduler 的 `market_only` 設定被完全忽略：無論使用者如何設定，`do_fetch_quotes()` 一律在非交易時間跳過抓取。此外，初始佔位符使用了 Montserrat 字型不支援的 `±` 符號，導致渲染為方框 `[]`。

## What Changes

- **修正 `market_only` 邏輯**：`do_fetch_quotes()` 改為只在 `s_config.market_only == true` 時才套用市場時段限制；`false` 時全天候抓取（TWSE API 休市時會回傳昨收價，現有的 `is_market_closed` 處理邏輯已可正確顯示）
- **修正 SNTP 競爭條件**：`scheduler_task` 在 WiFi 連線後立即呼叫 `do_fetch_quotes()`，但 SNTP 尚未同步完成，RTC 時間可能是 `2000-01-01 00:00:00`，導致市場時段判斷錯誤；改為等待 SNTP 同步後再執行第一次抓取（或給予合理等待視窗）
- **修正 `±` 字元**：Dashboard 初始佔位符改用 ASCII 相容的 `+/-` 或空字串，避免字型缺字顯示為方框
- **修正 AI 結果未推送 UI**：`main.c` 收到 AI queue 資料後僅記 log，沒有呼叫對應的 UI 更新函式

## Capabilities

### New Capabilities
- （無新功能，純修正）

### Modified Capabilities
- `scheduler`：`market_only` 的行為需求變更——由「永遠僅交易時段」改為「依設定值決定」
- `ui-manager`：AI 分析結果需實際反映到 UI（目前行為為只記 log）

## Impact

- `components/scheduler/scheduler.c`：`do_fetch_quotes()` 條件判斷修正、`scheduler_task` 首次抓取時機調整
- `components/ui/screens/screen_dashboard.c`：初始佔位符 `±0.00%` 字串修正
- `main/main.c`：AI result queue consumer 加入 UI 更新呼叫
- 不影響 NVS 資料格式、API 介面、硬體 HAL
