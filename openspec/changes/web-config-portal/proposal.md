## Why

目前 Core2 的設定（AI provider、股票清單、排程間隔等）只能透過裝置觸控螢幕操作，輸入不便且功能受限。Core2 已可連上 WiFi，具備在內網提供 HTTP 服務的條件。透過 Web 設定頁，使用者可在同一網段的瀏覽器完整設定裝置，顯著改善使用體驗。

## What Changes

- `device_server` 新增兩個獨立 API：`device_server_start_web_server()` / `device_server_stop_web_server()`，只啟動/停止 HTTP server，不觸碰 SoftAP；現有 `device_server_start_provisioning_portal()` 繼續同時啟動 SoftAP + HTTP server（共用同一份 HTTP server 程式碼）
- 新增 `screen_server` 頁面：顯示 Core2 內網 IP QR code（`http://<IP>`），load 時呼叫 `device_server_start_web_server()`，unload 時呼叫 `device_server_stop_web_server()`；WiFi 未連線時顯示提示訊息
- `screen_portal` 維持現有 SoftAP provisioning 行為，不做任何修改
- 中間鍵改為三點輪巡：Dashboard → Portal → Server → Dashboard（循環）
- 左右鍵僅在 `s_nav_screens[]`（Dashboard/Log/Info/Settings/HW_Test）內輪巡，不進入 Portal 或 Server

## Capabilities

### New Capabilities
- `screen-server`: 新 UI 頁面，顯示內網 IP QR code，load/unload 時啟動/停止 web server；WiFi 未連線時顯示提示訊息

### Modified Capabilities
- `device-server`: 新增 `device_server_start_web_server()` / `device_server_stop_web_server()`，將現有 HTTP server 的啟停邏輯從 SoftAP 解耦，供 Portal 與 Server screen 共用
- `hw-button-navigation`: 中間鍵行為改為三點輪巡（Dashboard/Portal/Server），左右鍵輪巡集合不含 Portal 及 Server

## Impact

- 修改：`components/device_server/device_server.c`（拆出 HTTP server 啟停為獨立函式，新增公開 API）
- 修改：`components/device_server/include/device_server.h`（新增 API 宣告）
- 新增：`components/ui/screens/screen_server.c`
- 修改：`components/ui/ui_manager.c`（中間鍵三點輪巡邏輯、新增 SCREEN_SERVER）
- 修改：`components/ui/CMakeLists.txt`（新增 screen_server.c）
- 無新增元件、無新增 NVS key、無新增分區
