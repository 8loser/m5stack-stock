# portal-stock-management Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
### Requirement: GET /stocks 回傳現有清單
`GET /stocks` SHALL 回傳目前 NVS 中儲存的股票清單，格式為 JSON 陣列，每個項目包含 `symbol`、`name`（中文全名）、`abbr`（英文簡稱）。若 meta cache 尚未填入，SHALL 對各 symbol 補呼叫 `twse_client_validate_symbol()` 填充 cache。

#### Scenario: 正常查詢
- **WHEN** 瀏覽器呼叫 `GET /stocks`
- **THEN** 回傳 `{"ok":true,"items":[{"symbol":"2330","name":"台積電","abbr":"TSMC"},...]}`

#### Scenario: 清單為空
- **WHEN** NVS 中 stocks count = 0
- **THEN** 回傳 `{"ok":true,"items":[]}`

### Requirement: POST /stocks/add 新增股票含驗證
`POST /stocks/add` SHALL 依序執行：格式驗證（4 位數字）→ 查重 → 上限（10 檔）→ WiFi 連線確認 → TWSE API 驗證（market=tse）。全部通過後儲存至 NVS，呼叫 `scheduler_reload_stock_list()`，並快取 meta（name / abbr）。

#### Scenario: 新增成功
- **WHEN** 送出 `{"symbol":"2330"}` 且驗證全部通過
- **THEN** 回傳 `{"ok":true,"item":{"symbol":"2330","name":"台積電","abbr":"TSMC","market":"tse"}}`，NVS 清單更新，下個排程週期生效

#### Scenario: 格式錯誤
- **WHEN** 送出 `{"symbol":"TSMC"}` 或非 4 位數字
- **THEN** 回傳 HTTP 400 `{"ok":false,"error":"invalid_format"}`

#### Scenario: 重複代號
- **WHEN** 送出已在清單中的 symbol
- **THEN** 回傳 HTTP 409 `{"ok":false,"error":"duplicate_symbol"}`

#### Scenario: 超過上限
- **WHEN** 清單已有 10 筆，再次新增
- **THEN** 回傳 HTTP 409 `{"ok":false,"error":"limit_exceeded"}`

#### Scenario: WiFi 離線
- **WHEN** 裝置目前未連上任何 AP
- **THEN** 回傳 HTTP 502 `{"ok":false,"error":"validate_failed"}`，不呼叫 TWSE API

#### Scenario: 代號不存在或非上市
- **WHEN** symbol 不在 TWSE 或 market != "tse"（如 OTC 股票）
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found_or_not_tse"}`

### Requirement: POST /stocks/remove 刪除股票
`POST /stocks/remove` SHALL 從 NVS 清單中移除指定 symbol，並呼叫 `scheduler_reload_stock_list()`。若 symbol 不在清單中回傳 404。

#### Scenario: 刪除成功
- **WHEN** 送出 `{"symbol":"2330"}` 且 symbol 在清單中
- **THEN** 回傳 `{"ok":true}`，NVS 清單更新，下個排程週期生效

#### Scenario: 代號不在清單
- **WHEN** 送出不在清單中的 symbol
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`

### Requirement: Portal 前端 Stocks tab
Portal 網頁 SHALL 提供 Stocks tab，顯示當前清單（symbol + name）、每列 Remove 按鈕、底部輸入框與 Add 按鈕。Add 動作 SHALL 在等待回應期間禁用按鈕，成功後重新載入清單。Portal 導覽列 SHALL 包含 WiFi、AI、Stocks 三個 tab，AI tab 用於儲存 API key（不觸發 AI 執行）。

#### Scenario: 新增股票
- **WHEN** 使用者在 Stocks tab 輸入 "2330" 並按 Add
- **THEN** 呼叫 `POST /stocks/add`，成功後清單刷新顯示新股票

#### Scenario: Remove 股票
- **WHEN** 使用者按下某股票列的 Remove 按鈕
- **THEN** 呼叫 `POST /stocks/remove`，成功後該列從清單消失

#### Scenario: AI tab 儲存 API key
- **WHEN** 使用者在 AI tab 填入 API key 並送出
- **THEN** 呼叫 `POST /ai`，key 儲存至 NVS，不觸發任何 AI 分析
