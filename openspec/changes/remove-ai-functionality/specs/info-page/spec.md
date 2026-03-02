## MODIFIED Requirements

### Requirement: Stocks section 顯示
Info 頁面 SHALL 顯示 Stocks section，包含：
- 排程參數：`Quote: <Xs>  Market-only: <Y/N>`（移除 AI interval）
- 股票清單：每支代號以空格分隔顯示於同一或多行；若清單為空顯示 "None"

#### Scenario: 有股票清單
- **WHEN** `storage_stocks_load()` 回傳 count > 0
- **THEN** 顯示所有 symbol，空格分隔，加上排程參數（不含 AI interval）

#### Scenario: 清單為空
- **WHEN** `storage_stocks_load()` 回傳 count = 0
- **THEN** 股票清單顯示 "None"

### Requirement: 可滾動佈局
Info 頁面 SHALL 使用可垂直捲動的容器（y=30, height=190px）容納三個 section（Device、Network、Stocks），當內容超出可視範圍時使用者可上下滑動瀏覽。

#### Scenario: 內容超出可視範圍
- **WHEN** 三個 section 總高度超過 190px
- **THEN** 使用者可向下滑動查看 Stocks section 完整內容

### Requirement: 進入頁面時一次性刷新
`screen_info_refresh()` SHALL 在每次切換至 Info 頁面時被呼叫一次，從各 API 讀取最新資料並更新所有 label。頁面停留期間不做週期性更新。

#### Scenario: 切換至 Info 頁面
- **WHEN** `ui_manager_switch_screen(SCREEN_INFO)` 被呼叫
- **THEN** `screen_info_refresh()` 在 mutex 持有期間執行，更新 6 個 label 文字（Device、Network、Stocks 各 2 個）

## REMOVED Requirements

### Requirement: AI section 顯示
**Reason**: AI 功能暫時移除，不再有 ai_provider 可查詢
**Migration**: 無，AI section 直接從頁面移除；Info 頁面改為 3 個 section
