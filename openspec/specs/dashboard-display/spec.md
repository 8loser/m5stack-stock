# dashboard-display Specification

## Purpose
TBD - created by syncing change fix-dashboard-display. Update Purpose after archive.

## Requirements
### Requirement: Stock symbol fully visible
Dashboard 的每張股票 card SHALL 完整顯示股票代碼文字，不得有任何像素被裁切。

#### Scenario: Symbol text not clipped
- **WHEN** Dashboard 顯示一筆股票報價
- **THEN** 股票代碼（如 "2330"）的所有像素 SHALL 完整可見，字型底部不被截斷

#### Scenario: Card name label two-line display
- **WHEN** `apply_card_widgets` 將 symbol + name 以換行格式寫入 name label
- **THEN** 兩行文字 SHALL 皆在 card 邊界內完整顯示

### Requirement: Next update time label
Dashboard 底部 SHALL 顯示下次報價排程的預計更新時間（絕對時間 HH:MM:SS）。

#### Scenario: Next time shown after quote arrives
- **WHEN** Dashboard 收到新的股票報價
- **THEN** 底部 "Next:" 標籤 SHALL 更新為下次報價觸發的絕對時間（台灣時區 CST+8）

#### Scenario: Next time reflects actual timer state
- **WHEN** scheduler 曾透過 `scheduler_trigger_quote_now()` 提前觸發
- **THEN** "Next:" 標籤 SHALL 反映 timer 重設後的實際到期時間，而非按 interval 推算的估計值

#### Scenario: Next time not shown before first quote
- **WHEN** 裝置剛開機且尚未收到任何報價
- **THEN** "Next:" 標籤 SHALL 顯示佔位文字（如 "Next: --:--:--"）

### Requirement: Dynamic stock card count
Dashboard SHALL 只顯示已設定的股票數量所對應的 card row，不顯示空白佔位 row。

#### Scenario: Exact match for configured count
- **WHEN** 使用者設定了 n 支股票（1 ≤ n ≤ 5）
- **THEN** Dashboard SHALL 在 storage 初始化後立即顯示 n 個可見 card row

#### Scenario: No stocks configured
- **WHEN** 未設定任何股票（count = 0）
- **THEN** Dashboard SHALL 不顯示任何 card row（全部 hidden），並 SHALL 顯示提示訊息引導使用者前往設定監測股票

#### Scenario: Empty state prompt content
- **WHEN** count = 0 且 Dashboard 為 active screen
- **THEN** 畫面中央 SHALL 顯示靜態提示文字（例如 "No stocks configured.\nGo to Settings to add."）

#### Scenario: Prompt hidden when stocks exist
- **WHEN** count ≥ 1
- **THEN** 提示文字 SHALL 不顯示（hidden）

#### Scenario: Count exceeds visible area — auto page flip
- **WHEN** 設定的股票數量超過 5（最多 10）
- **THEN** Dashboard SHALL 維持固定 5 列顯示，並每隔 `DASHBOARD_PAGE_FLIP_S` 秒切換到下一批 5 檔內容，循環回第一批

#### Scenario: Single page — no visible flip
- **WHEN** 設定的股票數量不超過 5
- **THEN** Dashboard SHALL 不切換頁面內容（維持單頁）

#### Scenario: Row count applied at boot without waiting for quote
- **WHEN** 裝置完成 `storage_init()` 後
- **THEN** Dashboard card 可見數量 SHALL 立即反映設定值，不需等待報價資料到達
