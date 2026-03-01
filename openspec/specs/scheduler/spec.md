# scheduler Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
### Requirement: scheduler_reload_stock_list 熱重載股票清單
`scheduler_reload_stock_list()` SHALL 從 NVS 重新載入股票清單，在下個排程週期開始時生效，不立即觸發報價抓取。

#### Scenario: 新增股票後熱重載
- **WHEN** `POST /stocks/add` 成功後呼叫 `scheduler_reload_stock_list()`
- **THEN** 下個排程週期的 `twse_client_fetch()` 包含新代號，不觸發立即抓取

#### Scenario: 刪除股票後熱重載
- **WHEN** `POST /stocks/remove` 成功後呼叫 `scheduler_reload_stock_list()`
- **THEN** 下個排程週期的 `twse_client_fetch()` 不包含已刪除代號

