## ADDED Requirements

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

## MODIFIED Requirements

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
