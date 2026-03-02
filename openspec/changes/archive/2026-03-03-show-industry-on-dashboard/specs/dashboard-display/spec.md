## ADDED Requirements

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
