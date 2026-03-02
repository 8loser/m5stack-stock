# scheduler Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
### Requirement: scheduler_init 不接受 AI queue 參數
`scheduler_init()` SHALL 只接受 `quote_queue` 一個 queue 參數，不再接受 `ai_result_queue`。

#### Scenario: 初始化排程器
- **WHEN** `scheduler_init(g_quote_queue)` 被呼叫
- **THEN** 排程器正常啟動，只建立 quote timer，不建立 AI timer

### Requirement: scheduler_reload_stock_list 熱重載股票清單
`scheduler_reload_stock_list()` SHALL 從 NVS 重新載入股票清單，在下個排程週期開始時生效，不立即觸發報價抓取。

#### Scenario: 新增股票後熱重載
- **WHEN** `POST /stocks/add` 成功後呼叫 `scheduler_reload_stock_list()`
- **THEN** 下個排程週期的 `twse_client_fetch()` 包含新代號，不觸發立即抓取

#### Scenario: 刪除股票後熱重載
- **WHEN** `POST /stocks/remove` 成功後呼叫 `scheduler_reload_stock_list()`
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
