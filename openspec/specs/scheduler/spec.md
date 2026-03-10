# scheduler Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
### Requirement: scheduler_service_init 不接受 AI queue 參數
`scheduler_service_init()` SHALL 只接受 `quote_queue` 一個 queue 參數，不再接受 `ai_result_queue`。

#### Scenario: 初始化排程器
- **WHEN** `scheduler_service_init(g_quote_queue)` 被呼叫
- **THEN** 排程器正常啟動，只建立 quote timer，不建立 AI timer

### Requirement: scheduler_service_reload_stock_list 熱重載股票清單
`scheduler_service_reload_stock_list()` SHALL 從 NVS 重新載入股票清單，在下個排程週期開始時生效，不立即觸發報價抓取。

#### Scenario: 新增股票後熱重載
- **WHEN** `POST /stocks/add` 成功後呼叫 `scheduler_service_reload_stock_list()`
- **THEN** 下個排程週期的 `twse_client_fetch()` 包含新代號，不觸發立即抓取

#### Scenario: 刪除股票後熱重載
- **WHEN** `POST /stocks/remove` 成功後呼叫 `scheduler_service_reload_stock_list()`
- **THEN** 下個排程週期的 `twse_client_fetch()` 不包含已刪除代號

### Requirement: market_only 設定控制報價抓取時段
排程器 SHALL 根據 `s_config.market_only` 決定是否限制在市場開市時段才抓取報價：
- `market_only = true`：非市場時段（`rtc_bm8563_is_market_open()` 返回 false）時跳過報價抓取
- `market_only = false`：全天候抓取，不受市場時段限制

此外，當 SNTP 尚未完成同步（`s_sntp_synced == false`）時，無論 `market_only` 設定為何，SHALL 跳過時段限制並直接抓取，以避免 RTC 時間不可信造成的誤判。

#### Scenario: market_only=true，非開市時間觸發排程
- **WHEN** quote timer 到期且 `s_config.market_only == true` 且 `rtc_bm8563_is_market_open()` 為 false
- **THEN** 本次報價抓取被跳過

#### Scenario: market_only=false，非開市時間觸發排程
- **WHEN** quote timer 到期且 `s_config.market_only == false`
- **THEN** 報價抓取正常執行，不受市場時段限制

#### Scenario: SNTP 尚未同步時觸發排程
- **WHEN** quote timer 到期且 `s_sntp_synced == false`（SNTP 尚未完成第一次同步）
- **THEN** 跳過市場時段判斷，報價抓取正常執行

#### Scenario: SNTP 已同步，market_only=true，開市時間
- **WHEN** `s_sntp_synced == true` 且 `s_config.market_only == true` 且 `rtc_bm8563_is_market_open()` 為 true
- **THEN** 報價抓取正常執行

### Requirement: scheduler_service_task 提供週期性活性心跳
`scheduler_service_task` SHALL 每輪主迴圈呼叫 `ui_manager_heartbeat_feed_scheduler()`，使 UI 可判斷排程器仍存活。

#### Scenario: scheduler service 正常運行時持續餵心跳
- **WHEN** `scheduler_service_task` 持續執行主迴圈
- **THEN** 每輪都呼叫 `ui_manager_heartbeat_feed_scheduler()` 一次

### Requirement: scheduler_service_task 等待粒度上限為 1 秒
`scheduler_service_task` 在等待通知時 SHALL 使用不超過 1000ms 的 timeout，以避免心跳更新間隔過長造成假性故障判定。

#### Scenario: 無通知時仍定期醒來
- **WHEN** `scheduler_service_task` 在該秒內未收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務最晚於 1000ms 內醒來，完成一次心跳更新與睡眠條件檢查

#### Scenario: 有通知時立即處理
- **WHEN** `scheduler_service_task` 在 timeout 前收到 `NOTIFY_QUOTE_BIT`
- **THEN** 任務立即處理報價抓取，且該輪仍會更新心跳

### Requirement: stock alert SHALL use threshold crossing with re-arm de-dup
系統在處理每筆最新報價時 SHALL 以 per-symbol、per-direction（up/down）狀態做「穿越觸發」去重：
- 只有從未達標到達標（false -> true crossing）時觸發
- 連續停留在達標區間時 SHALL NOT 重複觸發
- 回到門檻內後 SHALL 重新 armed，下一次再次穿越才可再觸發

#### Scenario: up threshold first crossing
- **WHEN** 某股票 `change_percent` 首次跨過 `up_threshold_pct`
- **THEN** 觸發一次 stock alert 事件

#### Scenario: stays above threshold
- **WHEN** 某股票連續多輪都維持在上漲門檻以上
- **THEN** 不重複觸發

#### Scenario: re-arm after returning inside threshold
- **WHEN** 某股票先回到門檻內，再次跨過同方向門檻
- **THEN** 再觸發一次

### Requirement: stock alert SHALL ignore market-closed snapshots
若報價為休市快照（`quote.is_market_closed == true`），stock alert 判斷流程 SHALL 直接略過，不做門檻觸發。

#### Scenario: market closed quote
- **WHEN** scheduler/main 收到 `is_market_closed=true` 的 quote
- **THEN** 不觸發 stock alert AI 呼叫與通知
