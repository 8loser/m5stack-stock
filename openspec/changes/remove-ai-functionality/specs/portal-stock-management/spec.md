## MODIFIED Requirements

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
