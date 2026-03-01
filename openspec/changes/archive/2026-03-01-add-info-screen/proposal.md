## Why

目前 UI 只有 Dashboard（股票行情）和 Portal（WiFi 配網）兩頁，使用者在裝置上無法查看裝置狀態、網路資訊、AI 設定與股票設定。需要一個唯讀的 Info 頁面統整這些資訊。

## What Changes

- 新增 `SCREEN_INFO = 2` 頁面，顯示四個 section：Device / Network / AI / Stocks
- 頁面採可滾動佈局，所有資料於進入時一次性刷新
- 導航：Dashboard 左鍵（btn=0）進入 Info，Info 左鍵返回 Dashboard
- Portal 進出行為不變（右鍵進、左鍵出）
- AI key 以末 4 碼 masked 顯示；未設定時顯示 "Not configured"

## Capabilities

### New Capabilities

- `info-page`: 裝置狀態資訊頁面，顯示 Device / Network / AI / Stocks 四個 section

### Modified Capabilities

- `ui-manager`: 新增 `SCREEN_INFO` enum 值、頁面初始化、switch_screen 刷新、hw_button 路由更新

## Impact

- `components/ui/include/ui_manager.h` — 修改：新增 `SCREEN_INFO` enum 及 `screen_info_create/refresh` 前向宣告
- `components/ui/screens/screen_info.c` — 新增：頁面建立、資料刷新、按鍵處理實作
- `components/ui/ui_manager.c` — 修改：s_screens 擴為 3、init 加 screen_info_create、switch_screen 加刷新、handle_hw_button 更新路由
- `components/ui/CMakeLists.txt` — 修改：SRCS 加 screen_info.c；REQUIRES 加 esp_system、esp_wifi
