# portal-stock-management Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
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

### Requirement: POST /stocks/remove 刪除股票
`POST /stocks/remove` SHALL 從 NVS 清單中移除指定 symbol，並呼叫 `scheduler_reload_stock_list()`。同一路徑 SHALL 刪除該 symbol 對應的 metadata（name / abbr / industry）。若 symbol 不在清單中回傳 404。

#### Scenario: 刪除成功
- **WHEN** 送出 `{"symbol":"2330"}` 且 symbol 在清單中
- **THEN** 回傳 `{"ok":true}`，NVS 清單與 metadata 皆移除，下個排程週期生效

#### Scenario: 代號不在清單
- **WHEN** 送出不在清單中的 symbol
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`

### Requirement: Portal 前端 Stocks tab
Portal 網頁 SHALL 提供 Stocks tab，顯示當前清單（symbol + name）、每列 Remove 按鈕、底部輸入框與 Add 按鈕。Add 動作 SHALL 在等待回應期間禁用按鈕，成功後重新載入清單。Portal 導覽列 SHALL 包含 WiFi、AI、Telegram、Stocks 四個 tab，AI tab 用於儲存 API key（不觸發 AI 執行）。Stocks 區塊提示文案 SHALL 清楚標示上限為 15 檔。

Portal 首次載入時 SHALL 顯示全頁初始化 overlay，直到初始化請求集合（`/scan`、`/ai`、`/telegram`、`/saved_aps`、`/stocks`）完成。若超過 timeout（預設 10000ms）仍未完成，系統 SHALL 顯示全頁錯誤面板與 Retry 控制，避免以空白內容誤導為「無資料」。

使用者按下 Retry 後，系統 SHALL 重跑全部初始化請求集合，並在全部完成後隱藏 overlay。系統 SHALL 防止舊輪次請求回應覆蓋新輪次狀態。

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

#### Scenario: 首次載入成功
- **WHEN** 使用者開啟 Portal 且初始化請求皆在 timeout 內完成
- **THEN** 頁面先顯示全頁 loading overlay，完成後隱藏並顯示可互動內容

#### Scenario: 載入逾時
- **WHEN** 任一初始化請求超過 timeout 尚未完成
- **THEN** 頁面顯示全頁錯誤面板與 Retry 按鈕，不以空白內容作為最終狀態

#### Scenario: Retry 重載
- **WHEN** 使用者在逾時錯誤面板點擊 Retry
- **THEN** 系統重跑全部初始化請求，並於完成後隱藏 overlay

#### Scenario: 連續重試下的競態保護
- **WHEN** 使用者連續觸發 Retry 且舊請求晚於新請求返回
- **THEN** 系統僅採用最新輪次結果更新 overlay 狀態

### Requirement: 每檔股票具備 alert_config 持久化設定
系統 SHALL 為每個 watchlist 股票維護獨立的 `alert_config`，包含 `enabled`、`up_threshold_pct`、`down_threshold_pct`、`ai_prompt`，並儲存於 NVS。若某股票尚未有設定資料，系統 SHALL 回傳預設值：`enabled=false`、`up_threshold_pct=0`、`down_threshold_pct=0`、`ai_prompt=""`。

#### Scenario: 既有股票首次讀取 alert_config
- **WHEN** 股票存在於清單，但 NVS 尚未有該股票的 alert config key
- **THEN** 系統回傳預設 alert_config，且 HTTP 200 成功

#### Scenario: 已儲存設定重啟後仍存在
- **WHEN** 使用者已更新某股票 alert_config，裝置重啟後再次讀取
- **THEN** 系統回傳與儲存時一致的 alert_config 內容

### Requirement: GET /stocks 回傳 alert_config
`GET /stocks` SHALL 在每個 `item` 內新增 `alert_config` 物件，欄位包含 `enabled`、`up_threshold_pct`、`down_threshold_pct`、`ai_prompt`。回傳中的 alert_config SHALL 對應目前 NVS 儲存值或預設值。

#### Scenario: 清單項目包含 alert_config
- **WHEN** 瀏覽器呼叫 `GET /stocks`
- **THEN** 回傳 `{"ok":true,"items":[...]}`
  且每個 item 均包含 `alert_config` 欄位

### Requirement: POST /stocks/update 更新單一股票設定
系統 SHALL 提供 `POST /stocks/update` 以更新單一股票的 alert_config。請求格式 SHALL 包含 `symbol` 與 `alert_config`。系統 SHALL 驗證 `symbol` 必須存在於目前清單，並驗證：
- `up_threshold_pct`、`down_threshold_pct` 為有限數值且範圍 0..99.99
- `ai_prompt` 長度不得超過 512 bytes（UTF-8）

驗證失敗 SHALL 回傳 HTTP 400；symbol 不存在 SHALL 回傳 HTTP 404；儲存失敗 SHALL 回傳 HTTP 500。

#### Scenario: 更新成功
- **WHEN** 請求內容合法且 symbol 在清單中
- **THEN** 系統儲存新 alert_config，回傳 `{"ok":true,"item":...}`，item 內含更新後 alert_config

#### Scenario: threshold 非法
- **WHEN** `up_threshold_pct` 或 `down_threshold_pct` 為負值、NaN 或大於 99.99
- **THEN** 回傳 HTTP 400 `{"ok":false,"error":"invalid_threshold"}`

#### Scenario: prompt 超過上限
- **WHEN** `ai_prompt` UTF-8 byte 長度 > 512
- **THEN** 回傳 HTTP 400 `{"ok":false,"error":"prompt_too_long"}`

#### Scenario: 股票不存在於清單
- **WHEN** `symbol` 不在目前 watchlist
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`

### Requirement: add/remove 同步管理 alert_config lifecycle
`POST /stocks/add` 成功新增股票後，系統 SHALL 同步建立該股票預設 alert_config。`POST /stocks/remove` 成功刪除股票後，系統 SHALL 一併刪除該股票的 alert_config 資料。

#### Scenario: 新增股票後存在預設設定
- **WHEN** `POST /stocks/add` 成功新增 symbol
- **THEN** 該 symbol 的 alert_config 已存在且為預設值（enabled=false）

#### Scenario: 刪除股票後設定同步移除
- **WHEN** `POST /stocks/remove` 成功刪除 symbol
- **THEN** 該 symbol 的 alert_config 資料同步從 NVS 移除

### Requirement: Portal Stocks tab 支援列表內展開編輯
Portal 前端 Stocks tab SHALL 在每檔股票列提供 `Edit`，展開後可編輯 `enabled`、`up_threshold_pct`、`down_threshold_pct`、`ai_prompt`，並提供 `Save` 與 `Cancel`。前端 SHALL 顯示提示文案：「單位時間依使用者設定的報價間隔」。

#### Scenario: 展開並儲存
- **WHEN** 使用者按下某股票 `Edit`，修改欄位後按 `Save`
- **THEN** 前端呼叫 `POST /stocks/update`，成功後更新該列摘要並顯示成功訊息

#### Scenario: 取消編輯
- **WHEN** 使用者在展開編輯區按 `Cancel`
- **THEN** 本次未儲存變更被捨棄，列表資料維持原值
