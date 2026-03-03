## Why

目前裝置螢幕亮度固定，在強光環境過暗、夜間環境過亮，影響使用體驗。AXP192 DCDC3 已可透過暫存器調整背光電壓，但 board HAL 層未暴露此介面，UI 亦無設定入口。

## What Changes

- 在 `board.h` 新增 `board_set_lcd_brightness(uint8_t level)` 包裝 AXP192 背光控制
- 在 Settings 頁面新增亮度 slider（0–100%），即時調整並持久化到 NVS
- NVS 新增 `display` namespace，key `brightness`（u8，預設 80）
- 開機初始化時從 NVS 讀取亮度值並套用

## Capabilities

### New Capabilities
- `lcd-brightness`: 螢幕亮度調整，含 board HAL 介面、NVS 持久化、開機套用

### Modified Capabilities
- `settings-page`: 新增亮度 slider UI 元件與對應儲存邏輯

## Impact

- `components/board/include/board.h` — 新增函式宣告
- `components/board/board.c` — 新增包裝實作
- `components/storage/` — 新增 `storage_display_save/load` 介面與 NVS 讀寫
- `components/ui/screens/screen_settings.c` — 新增 slider 元件
- `main/main.c` — 開機時呼叫亮度套用
- NVS namespace `display`，key `brightness`（u8）
