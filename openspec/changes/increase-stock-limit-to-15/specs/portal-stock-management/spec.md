## MODIFIED Requirements

### Requirement: POST /stocks/add 新增股票含驗證
`POST /stocks/add` SHALL 依序執行：格式驗證（4 位數字）→ 查重 → 上限（15 檔）→ WiFi 連線確認 → TWSE API 驗證（market=tse）。全部通過後儲存至 NVS，呼叫 `scheduler_reload_stock_list()`，並快取 meta（name / abbr）。系統同時 SHALL 觸發股票清單變更通知，將最新股票數量同步給 dashboard card count。

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
- **WHEN** 清單已有 15 筆，再次新增
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

### Requirement: Portal 前端 Stocks tab
Portal 網頁 SHALL 提供 Stocks tab，顯示當前清單（symbol + name）、每列 Remove 按鈕、底部輸入框與 Add 按鈕。Add 動作 SHALL 在等待回應期間禁用按鈕，成功後重新載入清單。Portal 導覽列 SHALL 包含 WiFi、AI、Stocks 三個 tab，AI tab 用於儲存 API key（不觸發 AI 執行）。Stocks 區塊提示文案 SHALL 清楚標示上限為 15 檔。

#### Scenario: 新增股票
- **WHEN** 使用者在 Stocks tab 輸入 "2330" 並按 Add
- **THEN** 呼叫 `POST /stocks/add`，成功後清單刷新顯示新股票

#### Scenario: Remove 股票
- **WHEN** 使用者按下某股票列的 Remove 按鈕
- **THEN** 呼叫 `POST /stocks/remove`，成功後該列從清單消失

#### Scenario: AI tab 儲存 API key
- **WHEN** 使用者在 AI tab 填入 API key 並送出
- **THEN** 呼叫 `POST /ai`，key 儲存至 NVS，不觸發任何 AI 分析

#### Scenario: 顯示上限提示為 15
- **WHEN** 使用者開啟 Portal 的 Stocks tab
- **THEN** 頁面標題或提示文字 SHALL 顯示股票上限為 15 檔
