## MODIFIED Requirements

### Requirement: POST /stocks/add 新增股票含驗證
`POST /stocks/add` SHALL 依序執行：格式驗證（4 位數字）→ 查重 → 上限（10 檔）→ WiFi 連線確認 → TWSE API 驗證（market=tse）。全部通過後儲存至 NVS，呼叫 `scheduler_reload_stock_list()`，並快取 meta（name / abbr）。系統同時 SHALL 觸發股票清單變更通知，將最新股票數量同步給 dashboard card count。

#### Scenario: 新增成功
- **WHEN** 送出 `{"symbol":"2330"}` 且驗證全部通過
- **THEN** 回傳 `{"ok":true,"item":{"symbol":"2330","name":"台積電","abbr":"TSMC","market":"tse"}}`，NVS 清單更新，下個排程週期生效，且 dashboard card count 立即同步為新 count

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

#### Scenario: 新增失敗不更新 dashboard count
- **WHEN** 新增流程在驗證或儲存任一步驟失敗
- **THEN** 系統 SHALL NOT 觸發股票清單變更通知，dashboard card count 維持原值

### Requirement: POST /stocks/remove 刪除股票
`POST /stocks/remove` SHALL 從 NVS 清單中移除指定 symbol，並呼叫 `scheduler_reload_stock_list()`。若 symbol 不在清單中回傳 404。刪除成功後，系統 SHALL 觸發股票清單變更通知，將最新股票數量同步給 dashboard card count。

#### Scenario: 刪除成功
- **WHEN** 送出 `{"symbol":"2330"}` 且 symbol 在清單中
- **THEN** 回傳 `{"ok":true}`，NVS 清單更新，下個排程週期生效，且 dashboard card count 立即同步為新 count

#### Scenario: 代號不在清單
- **WHEN** 送出不在清單中的 symbol
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`

#### Scenario: 刪除失敗不更新 dashboard count
- **WHEN** symbol 不存在或儲存失敗
- **THEN** 系統 SHALL NOT 觸發股票清單變更通知，dashboard card count 維持原值
