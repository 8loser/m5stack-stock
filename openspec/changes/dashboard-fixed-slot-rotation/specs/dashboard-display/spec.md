## MODIFIED Requirements

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

## ADDED Requirements

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
