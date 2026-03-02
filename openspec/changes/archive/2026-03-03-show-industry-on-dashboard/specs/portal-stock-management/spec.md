## MODIFIED Requirements

### Requirement: GET /stocks 回傳現有清單
`GET /stocks` SHALL 回傳目前 NVS 中儲存的股票清單，格式為 JSON 陣列，每個項目包含 `symbol`、`name`（中文全名）、`abbr`（英文簡稱）與 `industry`（產業別）。當 metadata 存在時 SHALL 直接回傳已保存的欄位；當 metadata 不存在時 SHALL 回傳空字串，不得在 `GET /stocks` 內觸發產業補抓。

#### Scenario: 正常查詢
- **WHEN** 瀏覽器呼叫 `GET /stocks`
- **THEN** 回傳 `{"ok":true,"items":[{"symbol":"2330","name":"台積電","abbr":"TSMC","industry":"半導體"},...]}`

#### Scenario: 清單為空
- **WHEN** NVS 中 stocks count = 0
- **THEN** 回傳 `{"ok":true,"items":[]}`

#### Scenario: metadata 不完整
- **WHEN** 某 symbol 尚未保存 `industry`
- **THEN** 該項目 `industry` SHALL 為空字串，且請求仍回傳 HTTP 200

### Requirement: POST /stocks/add 新增股票含驗證
`POST /stocks/add` SHALL 依序執行：格式驗證（4 位數字）→ 查重 → 上限（10 檔）→ WiFi 連線確認 → TWSE API 驗證（market=tse）。全部通過後 SHALL 儲存 symbol 至 NVS，並在同一成功路徑執行 metadata enrich（`name` / `abbr` / `industry`）後寫入 cache 與 metadata storage，接著呼叫 `scheduler_reload_stock_list()`。

#### Scenario: 新增成功且取得產業別
- **WHEN** 送出 `{"symbol":"2330"}` 且驗證與 metadata enrich 全部通過
- **THEN** 回傳 `{"ok":true,"item":{"symbol":"2330","name":"台積電","abbr":"TSMC","industry":"半導體","market":"tse"}}`，NVS 清單與 metadata 皆更新

#### Scenario: 新增成功但產業別取得失敗
- **WHEN** symbol 驗證成功但 metadata enrich 未取得 `industry`
- **THEN** 新增流程 SHALL 仍成功，回傳 `industry` 空字串（或系統定義未知值）

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
`POST /stocks/remove` SHALL 從 NVS 清單中移除指定 symbol，並呼叫 `scheduler_reload_stock_list()`。同一路徑 SHALL 刪除該 symbol 對應的 metadata（name / abbr / industry）。若 symbol 不在清單中回傳 404。

#### Scenario: 刪除成功
- **WHEN** 送出 `{"symbol":"2330"}` 且 symbol 在清單中
- **THEN** 回傳 `{"ok":true}`，NVS 清單與 metadata 皆移除，下個排程週期生效

#### Scenario: 代號不在清單
- **WHEN** 送出不在清單中的 symbol
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`
