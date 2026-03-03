## Why

目前 dashboard 欄位順序為 Name / Industry / Price / Change%，將輔助性資訊（產業別）擺在財務關鍵資料（現價、漲跌幅）之前，視覺優先順序與使用者關注順序不符。將產業別移至第四欄，可讓使用者第一眼看到最重要的財務數據。

## What Changes

- 重排 dashboard 四欄順序：Name | Price | Change% | Industry（原為 Name | Industry | Price | Change%）
- 調整各欄寬度比例，依內容特性分配（Name 加寬、Change% 縮窄、Industry 使用剩餘空間）

## Capabilities

### New Capabilities

（無新增 capability）

### Modified Capabilities

- `dashboard-display`: 欄位順序與寬度分配規則變更

## Impact

- 只影響 `components/ui/screens/screen_dashboard.c`
  - `compute_card_layout()`：欄寬分配邏輯
  - `ensure_card_widgets()`：各 label widget 的 pos/size 設定
- 不影響資料流、NVS、排程、或其他元件
