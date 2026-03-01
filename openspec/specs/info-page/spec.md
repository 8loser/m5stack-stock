# info-page Specification

## Purpose
TBD - created by archiving change add-info-screen. Update Purpose after archive.
## Requirements
### Requirement: Device section 顯示
Info 頁面 SHALL 顯示 Device section，包含以下欄位：
- `Heap: <N> bytes`（`esp_get_free_heap_size()` 回傳值）
- `Chip: <model> <cores>-core`（`esp_chip_info()` 回傳的 model 與 cores）
- `IDF: <version>`（`esp_get_idf_version()` 回傳的版本字串）

#### Scenario: 正常顯示裝置資訊
- **WHEN** 使用者進入 Info 頁面
- **THEN** Device section 顯示當前可用 heap、晶片型號核心數、IDF 版本

### Requirement: Network section 顯示
Info 頁面 SHALL 顯示 Network section，包含：
- `SSID: <ssid>`（從 NVS `storage_wifi_load()` 讀取；無設定則顯示 "Not saved"）
- `State: <Connected|Connecting|Offline>`（`wifi_manager_get_state()` 對應文字）
- `IP: <ip>`（`wifi_manager_get_ip()`；NULL 或未連線則顯示 "--"）

#### Scenario: WiFi 已連線
- **WHEN** `wifi_manager_get_state()` 回傳 `WIFI_STATE_CONNECTED`
- **THEN** State 顯示 "Connected"，IP 顯示實際 IP 位址

#### Scenario: WiFi 未連線
- **WHEN** `wifi_manager_get_state()` 回傳非 CONNECTED 狀態
- **THEN** State 顯示對應文字（"Connecting" 或 "Offline"），IP 顯示 "--"

#### Scenario: 無已儲存 WiFi 設定
- **WHEN** `storage_wifi_load()` 回傳錯誤或 SSID 為空
- **THEN** SSID 顯示 "Not saved"

### Requirement: AI section 顯示
Info 頁面 SHALL 顯示 AI section，包含：
- `Provider: <Gemini|Claude|OpenAI>`（`ai_provider_get_name()` 回傳值）
- `Key: ****<last4>`（`ai_provider_get_active_api_key()` 讀取後遮蔽，僅顯示末 4 碼）；
  key 長度 < 4 且非空顯示 "****"；完全空白顯示 "Not configured"

#### Scenario: API key 已設定
- **WHEN** NVS 中有長度 ≥ 4 的 API key
- **THEN** 顯示 `Key: ****XXXX`（XXXX 為末 4 碼）

#### Scenario: API key 未設定
- **WHEN** NVS 中無 API key（空字串）
- **THEN** 顯示 `Key: Not configured`

### Requirement: Stocks section 顯示
Info 頁面 SHALL 顯示 Stocks section，包含：
- 排程參數：`Quote: <Xs>  AI: <Xmin>  Market-only: <Y/N>`
- 股票清單：每支代號以空格分隔顯示於同一或多行；若清單為空顯示 "None"

#### Scenario: 有股票清單
- **WHEN** `storage_stocks_load()` 回傳 count > 0
- **THEN** 顯示所有 symbol，空格分隔，加上排程參數

#### Scenario: 清單為空
- **WHEN** `storage_stocks_load()` 回傳 count = 0
- **THEN** 股票清單顯示 "None"

### Requirement: 可滾動佈局
Info 頁面 SHALL 使用可垂直捲動的容器（y=30, height=190px）容納四個 section，當內容超出可視範圍時使用者可上下滑動瀏覽。

#### Scenario: 內容超出可視範圍
- **WHEN** 四個 section 總高度超過 190px
- **THEN** 使用者可向下滑動查看 Stocks section 完整內容

### Requirement: 進入頁面時一次性刷新
`screen_info_refresh()` SHALL 在每次切換至 Info 頁面時被呼叫一次，從各 API 讀取最新資料並更新所有 label。頁面停留期間不做週期性更新。

#### Scenario: 切換至 Info 頁面
- **WHEN** `ui_manager_switch_screen(SCREEN_INFO)` 被呼叫
- **THEN** `screen_info_refresh()` 在 mutex 持有期間執行，更新 8 個 label 文字

