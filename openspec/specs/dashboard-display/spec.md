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

### Requirement: Card accent border reflects price direction
每張股票 card SHALL 在左側顯示 4px 彩色邊框以反映漲跌方向，背景色統一不再隨漲跌改變。

#### Scenario: Rising stock shows red left border
- **WHEN** `change_percent > 0.01`
- **THEN** card 左側邊框 SHALL 顯示暖紅色（`#FF3B30`），card 背景 SHALL 為統一深藍（`#1C2A4A`）

#### Scenario: Falling stock shows green left border
- **WHEN** `change_percent < -0.01`
- **THEN** card 左側邊框 SHALL 顯示清綠色（`#30D158`），card 背景 SHALL 為統一深藍（`#1C2A4A`）

#### Scenario: Flat stock shows grey left border
- **WHEN** `-0.01 <= change_percent <= 0.01`
- **THEN** card 左側邊框 SHALL 顯示中灰色（`#555555`），card 背景 SHALL 為統一深藍（`#1C2A4A`）

#### Scenario: No bg tint on any card
- **WHEN** Dashboard 顯示任意漲跌狀態的股票
- **THEN** card 背景色 SHALL 統一為 `COLOR_CARD`，不得出現紅色或綠色背景 tint

### Requirement: Name label left margin respects border width
股票名稱標籤 SHALL 從 card 左側第 10px 開始，讓出左側邊框的視覺空間。

#### Scenario: Name label x offset
- **WHEN** Dashboard 顯示股票卡片
- **THEN** name label 的 x 座標 SHALL 為 10（原為 4），y 座標 SHALL 為 2

### Requirement: Next update time label
Dashboard 底部 SHALL 在同一行顯示最後更新時間（左側）與下次報價排程的預計更新時間（右側）；不再以兩行分置。最後更新時間標籤文字格式為 `Upd HH:MM:SS`，下次更新時間標籤文字格式為 `Next HH:MM:SS`。

#### Scenario: Both labels shown on same row after quote arrives
- **WHEN** Dashboard 收到新的股票報價
- **THEN** 底部 SHALL 同行顯示左側 `Upd HH:MM:SS`（最後更新）與右側 `Next HH:MM:SS`（下次排程），兩者 y 座標 SHALL 相同

#### Scenario: Next time reflects actual timer state
- **WHEN** scheduler 曾透過 `scheduler_trigger_quote_now()` 提前觸發
- **THEN** "Next" 標籤 SHALL 反映 timer 重設後的實際到期時間，而非按 interval 推算的估計值

#### Scenario: Placeholder shown before first quote
- **WHEN** 裝置剛開機且尚未收到任何報價
- **THEN** 兩個標籤 SHALL 顯示佔位文字（`Upd --:--:--` 及 `Next: --:--:--`）

#### Scenario: Labels do not overflow screen width
- **WHEN** 兩個標籤同行顯示
- **THEN** 兩者文字 SHALL 不互相重疊，且皆在 `LCD_WIDTH` 範圍內

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
