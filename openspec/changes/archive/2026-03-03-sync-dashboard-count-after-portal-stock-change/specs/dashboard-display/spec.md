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

#### Scenario: Row count applied after portal stock add
- **WHEN** 使用者在 Portal 成功新增股票並切回 Dashboard
- **THEN** Dashboard card 可見數量 SHALL 立即反映新增後 count，不需等待下一筆報價

#### Scenario: Row count applied after portal stock removal
- **WHEN** 使用者在 Portal 成功刪除股票並切回 Dashboard
- **THEN** Dashboard card 可見數量 SHALL 立即反映刪除後 count，不需等待下一筆報價
