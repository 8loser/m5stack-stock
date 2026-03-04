## MODIFIED Requirements

### Requirement: Portal 前端 Stocks tab
Portal 網頁 SHALL 提供 Stocks tab，顯示當前清單（symbol + name）、每列 Remove 按鈕、底部輸入框與 Add 按鈕。Add 動作 SHALL 在等待回應期間禁用按鈕，成功後重新載入清單。Portal 導覽列 SHALL 包含 WiFi、AI、Telegram、Stocks 四個 tab，AI tab 用於儲存 API key（不觸發 AI 執行）。Stocks 區塊提示文案 SHALL 清楚標示上限為 15 檔。

Portal 首次載入時 SHALL 顯示全頁初始化 overlay，直到初始化請求集合（`/scan`、`/ai`、`/telegram`、`/saved_aps`、`/stocks`）完成。若超過 timeout（預設 10000ms）仍未完成，系統 SHALL 顯示全頁錯誤面板與 Retry 控制，避免以空白內容誤導為「無資料」。

使用者按下 Retry 後，系統 SHALL 重跑全部初始化請求集合，並在全部完成後隱藏 overlay。系統 SHALL 防止舊輪次請求回應覆蓋新輪次狀態。

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
