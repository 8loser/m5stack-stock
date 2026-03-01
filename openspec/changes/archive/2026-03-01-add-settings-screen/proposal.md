## Why

使用者希望能在裝置上直接調整股價查詢輪詢間隔（quote_interval_s），目前必須透過韌體重新燒錄才能變更。現有 `scheduler_apply_config()` 與 `storage_schedule_save/load()` 已支援動態設定，缺少 UI 入口。

## What Changes

- 新增 `SCREEN_SETTINGS = 4` 頁面，含 3 個 interval 按鈕（1min / 5min / 10min）
- 點擊 interval 按鈕即立即套用並儲存設定，畫面顯示儲存成功/失敗訊息
- 移除 Settings 頁面的 `Market Hours Only` 控制與 `Save/Cancel` 按鈕
- 導航：透過 Core2 既有硬體按鍵循環（btn=0/2）可切換至 Settings；Portal 進出行為不變
- 排程策略改為固定：非開市時間不抓價格（不再由 UI 控制）

## Capabilities

### New Capabilities

- `settings-page`: 排程設定頁面，支援輪詢間隔一鍵儲存與結果提示

### Modified Capabilities

- `ui-manager`: 新增 `SCREEN_SETTINGS` enum 值、頁面初始化、硬體按鍵輪詢路由更新
- `scheduler`: 固定於非開市時間跳過報價抓取

## Impact

- `components/ui/screens/screen_settings.c` — 新增：interval 按鈕佈局、載入 scheduler_get_config、即時儲存與提示訊息
- `components/ui/include/ui_manager.h` — 修改：新增 `SCREEN_SETTINGS` enum
- `components/ui/ui_manager.c` — 修改：s_screens 擴為 5、init 加 screen_settings_create、switch_screen 加載入邏輯、`s_nav_screens` 納入 `SCREEN_SETTINGS`
- `components/ui/CMakeLists.txt` — 修改：SRCS 加 screen_settings.c
- `components/scheduler/scheduler.c` — 修改：非開市固定跳過報價抓取
