## ADDED Requirements

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
