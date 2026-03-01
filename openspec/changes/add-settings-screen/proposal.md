## Why

使用者希望能在裝置上直接調整股價查詢輪詢間隔（quote_interval_s）與市場時段限制（market_only），目前必須透過韌體重新燒錄才能變更這些排程參數。現有 `scheduler_apply_config()` 與 `storage_schedule_save/load()` 已支援動態設定，缺少 UI 入口。

## What Changes

- 新增 `SCREEN_SETTINGS = 2` 頁面，含 `lv_roller` 選擇輪詢間隔（30s / 1min / 2min / 5min / 10min）與 `lv_checkbox` 切換 Market Hours Only
- 按鍵語義：btn=0（BtnA）不存檔返回 Dashboard，btn=1（BtnB）儲存設定後返回 Dashboard
- 導航：Dashboard 左鍵（btn=0）進入 Settings；Portal 進出行為不變

## Capabilities

### New Capabilities

- `settings-page`: 排程設定頁面，支援輪詢間隔選擇與市場時段限制開關，儲存後即時生效

### Modified Capabilities

- `ui-manager`: 新增 `SCREEN_SETTINGS` enum 值、頁面初始化、hw_button 路由更新

## Impact

- `components/ui/screens/screen_settings.c` — 新增：roller / checkbox 佈局、載入 scheduler_get_config、儲存 scheduler_apply_config
- `components/ui/include/ui_manager.h` — 修改：新增 `SCREEN_SETTINGS` enum
- `components/ui/ui_manager.c` — 修改：s_screens 擴為 3、init 加 screen_settings_create、switch_screen guard 改為 >= 3、handle_hw_button 更新路由
- `components/ui/CMakeLists.txt` — 修改：SRCS 加 screen_settings.c
