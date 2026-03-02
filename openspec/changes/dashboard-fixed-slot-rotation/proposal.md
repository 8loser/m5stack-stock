## Why

目前 Dashboard 在股票數量超過 5 檔時使用整頁翻頁，畫面會整批跳動，不符合使用者希望的「固定格位看盤」體驗。需要改為固定 5 格位置、以節拍輪巡替換內容，提升可讀性並降低視覺負擔。

## What Changes

- 將 Dashboard 超過 5 檔時的顯示策略從「整頁翻頁」改為「固定格位輪巡換檔」。
- 新增固定 UI 節拍刷新（預設 2 秒），與報價抓取排程解耦。
- 定義輪巡規則：每個節拍只替換 1 格，依股票設定清單順序循環。
- 定義切回 Dashboard 的行為：重置輪巡指標，從第 1 檔重新開始。
- 保持 card 漲跌配色與當前顯示資料一致（替換到哪一檔就用該檔漲跌色）。

## Capabilities

### New Capabilities
- `dashboard-slot-rotation`: Dashboard 固定格位輪巡顯示與節拍控制。

### Modified Capabilities
- `dashboard-display`: 將超量股票顯示需求從整頁翻頁改為固定格位輪巡，並補充顏色同步規範。

## Impact

- Affected code:
  - `components/ui/screens/screen_dashboard.c`
  - `components/ui/ui_manager.c`
  - `components/ui/include/ui_manager.h`
  - `include/app_config.h`
- No external API change。
- 影響 Dashboard UI 行為與使用者可見顯示節奏。
