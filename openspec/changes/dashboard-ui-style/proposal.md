## Why

Dashboard 卡片的漲跌視覺區分度不足：背景 tint（`#2D0000`/`#002D00`）在深藍底色上幾乎不可見；Footer 兩行中文混排 Montserrat 佔用垂直空間；卡片左邊距未留出邊框空間導致視覺層次不清晰。

## What Changes

- 以左側 4px accent border 取代背景 tint，漲跌平分別用紅/綠/灰邊框標示
- 更新配色常數：`COLOR_UP`、`COLOR_DOWN`、`COLOR_CARD`、新增 `COLOR_FLAT`（border 用灰）
- 名稱標籤 x 偏移從 `x=4` 改為 `x=10`，讓出邊框視覺空間
- Footer 由兩行改為同行左右分置：左側 `Upd HH:MM:SS`，右側 `Next HH:MM:SS`
- 「更新:」文字改為 `Upd`（英文，統一字型，節省空間）

## Capabilities

### New Capabilities
<!-- 此改動為純 style 調整，不引入新的業務 capability -->

### Modified Capabilities
- `dashboard-display`: 卡片視覺樣式變更（border side/color 取代 bg tint）；footer layout 由兩行改為單行左右分置

## Impact

- 只改 `components/ui/screens/screen_dashboard.c`，不影響其他檔案
- 不改任何資料流、邏輯或 API
- 依賴 LVGL 8.4 的 `LV_BORDER_SIDE_LEFT` 支援（已在專案中使用的版本）
