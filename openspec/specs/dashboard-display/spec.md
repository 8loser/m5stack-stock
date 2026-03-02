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
Dashboard SHALL 只顯示已設定的股票數量所對應資料，並在超過可視列數時使用固定格位輪巡，不得整頁翻頁。

#### Scenario: Exact match for configured count
- **WHEN** 使用者設定了 n 支股票（1 ≤ n ≤ 5）
- **THEN** Dashboard SHALL 在 storage 初始化後立即顯示 n 個可見 card row

#### Scenario: No stocks configured
- **WHEN** 未設定任何股票（count = 0）
- **THEN** Dashboard SHALL 不顯示任何 card row（全部 hidden），並 SHALL 顯示提示訊息引導使用者前往設定監測股票

#### Scenario: Prompt hidden when stocks exist
- **WHEN** count ≥ 1
- **THEN** 提示文字 SHALL 不顯示（hidden）

#### Scenario: Count exceeds visible area uses fixed-slot rotation
- **WHEN** 設定的股票數量超過 5（最多 10）
- **THEN** Dashboard SHALL 維持固定 5 列位置，並在固定 UI 節拍下每次只替換 1 列內容為下一檔股票，循環回第一檔

#### Scenario: No page-level flip animation
- **WHEN** 設定的股票數量超過 5
- **THEN** Dashboard SHALL NOT 以整頁切換方式更新顯示內容

#### Scenario: Row count applied at boot without waiting for quote
- **WHEN** 裝置完成 `storage_init()` 後
- **THEN** Dashboard card 可見數量 SHALL 立即反映設定值，不需等待報價資料到達

#### Scenario: Row count applied after portal stock add
- **WHEN** 使用者在 Portal 成功新增股票並切回 Dashboard
- **THEN** Dashboard card 可見數量 SHALL 立即反映新增後 count，不需等待下一筆報價

#### Scenario: Row count applied after portal stock removal
- **WHEN** 使用者在 Portal 成功刪除股票並切回 Dashboard
- **THEN** Dashboard card 可見數量 SHALL 立即反映刪除後 count，不需等待下一筆報價

### Requirement: Slot color reflects currently displayed quote
每個固定格位在刷新內容時，SHALL 使用該格當前顯示股票的漲跌資料更新顏色，避免顏色與內容不一致。

#### Scenario: Rising quote applies rising accent on refreshed slot
- **WHEN** 某格位刷新到 `change_percent > 0.01` 的股票
- **THEN** 該格位漲跌文字與左側邊框 SHALL 顯示上漲色

#### Scenario: Falling quote applies falling accent on refreshed slot
- **WHEN** 某格位刷新到 `change_percent < -0.01` 的股票
- **THEN** 該格位漲跌文字與左側邊框 SHALL 顯示下跌色

#### Scenario: Flat quote applies neutral accent on refreshed slot
- **WHEN** 某格位刷新到 `-0.01 <= change_percent <= 0.01` 的股票
- **THEN** 該格位漲跌文字與左側邊框 SHALL 顯示平盤色

#### Scenario: Rotation refresh keeps color-content consistency
- **WHEN** UI 節拍觸發固定格位輪巡替換內容
- **THEN** 新內容顯示完成後，同一格位的顏色 SHALL 對應新內容的漲跌狀態

### Requirement: Dashboard card uses four-column layout
Dashboard 股票卡片 SHALL 以四欄顯示：`symbol+name | industry | price | change%`。其中左欄保留兩行（symbol 在上、name 在下），第二欄顯示產業別，第三欄顯示價格，第四欄顯示漲跌幅。

#### Scenario: Quote with industry renders four columns
- **WHEN** Dashboard 收到包含 `industry` 的股票資料
- **THEN** 卡片 SHALL 顯示四欄資訊，且產業別位於第二欄

#### Scenario: Quote without industry still renders correctly
- **WHEN** Dashboard 收到 `industry` 為空字串的股票資料
- **THEN** 第二欄 SHALL 顯示空字串或預設未知值，且不影響其餘三欄顯示

### Requirement: Industry column must not break price/change readability
Dashboard 第二欄（industry）SHALL 使用固定欄寬與截斷策略，避免擠壓第三、四欄資訊。price 與 change% SHALL 維持可完整讀取。

#### Scenario: Long industry text
- **WHEN** 產業別文字超過第二欄可用寬度
- **THEN** 第二欄文字 SHALL 被截斷（例如 ellipsis），price 與 change% 欄位不重疊且完整可見
