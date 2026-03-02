## 1. 改名 wifi_manager → device_server

- [ ] 1.1 重新命名目錄 `components/wifi_manager/` → `components/device_server/`
- [ ] 1.2 重新命名 `wifi_manager.c` → `device_server.c`，`wifi_manager.h` → `device_server.h`
- [ ] 1.3 更新 `components/device_server/CMakeLists.txt`：元件名稱、SRCS 檔名
- [ ] 1.4 在 `device_server.c` 中將所有 `wifi_manager_` 函式前綴替換為 `device_server_`
- [ ] 1.5 在 `device_server.h` 中將所有宣告前綴替換為 `device_server_`
- [ ] 1.6 更新 `components/ui/screens/screen_portal.c`：include 路徑改為 `device_server.h`，所有 API 呼叫改為 `device_server_*`
- [ ] 1.7 更新 `components/ui/screens/screen_info.c`：include 路徑改為 `device_server.h`，所有 API 呼叫改為 `device_server_*`
- [ ] 1.8 更新 `components/ui/CMakeLists.txt`：依賴改為 `device_server`
- [ ] 1.9 更新 `main/CMakeLists.txt`：依賴改為 `device_server`
- [ ] 1.10 更新 `main/main.c`：include 路徑改為 `device_server.h`，所有 API 呼叫改為 `device_server_*`

## 2. HTTP server refcount 解耦

- [ ] 2.1 在 `device_server.c` 新增 `static int s_web_server_refcount = 0`
- [ ] 2.2 實作 `device_server_start_web_server()`：refcount +1，若為第一次呼叫則啟動 httpd，回傳 `ESP_OK`
- [ ] 2.3 實作 `device_server_stop_web_server()`：refcount -1，若 refcount 歸零則停止 httpd
- [ ] 2.4 更新 `start_provisioning_portal()` 改呼叫 `device_server_start_web_server()` 取代直接呼叫 `start_portal_http_server()`
- [ ] 2.5 更新 `stop_provisioning_portal()` 改呼叫 `device_server_stop_web_server()` 取代直接呼叫 `stop_portal_http_server()`
- [ ] 2.6 將 `start_portal_http_server()` / `stop_portal_http_server()` 設為純 static 內部輔助函式（不對外暴露）
- [ ] 2.7 在 `device_server.h` 新增 `device_server_start_web_server()` 與 `device_server_stop_web_server()` 宣告

## 3. 新增 screen_server

- [ ] 3.1 建立 `components/ui/screens/screen_server.c`，實作 `screen_server_create()` 回傳 `lv_obj_t *`
- [ ] 3.2 實作進入頁面時呼叫 `esp_netif_get_ip_info()` 取得 STA IP
- [ ] 3.3 若 IP 有效（不為 0.0.0.0），生成 QR code（內容 `http://A.B.C.D`）並顯示 URL 文字標籤
- [ ] 3.4 若 IP 無效，顯示提示訊息「Not connected to WiFi」，隱藏 QR code
- [ ] 3.5 實作 `screen_server_on_load()`：WiFi 已連線時呼叫 `device_server_start_web_server()`
- [ ] 3.6 實作 `screen_server_on_unload()`：呼叫 `device_server_stop_web_server()`
- [ ] 3.7 在 `components/ui/CMakeLists.txt` 的 SRCS 中加入 `screens/screen_server.c`

## 4. ui_manager 整合 SCREEN_SERVER 與按鍵邏輯

- [ ] 4.1 在 `screen_id_t` enum 新增 `SCREEN_SERVER`（置於 `SCREEN_PORTAL` 之後）
- [ ] 4.2 在 `ui_manager_init()` 呼叫 `screen_server_create()` 並存入 `s_screens[SCREEN_SERVER]`
- [ ] 4.3 在 `ui_manager_switch_screen()` 新增 SCREEN_SERVER 的 load/unload hook（呼叫 `screen_server_on_load/unload()`）
- [ ] 4.4 新增 `static const screen_id_t s_mid_screens[] = {SCREEN_DASHBOARD, SCREEN_PORTAL, SCREEN_SERVER}` 與 `static int s_mid_btn_idx = 0`
- [ ] 4.5 修改 `handle_hw_button()` 中間鍵（btn 1）邏輯：改為 `s_mid_btn_idx = (s_mid_btn_idx + 1) % 3`，切換至 `s_mid_screens[s_mid_btn_idx]`
- [ ] 4.6 修改 `handle_hw_button()`：在 `SCREEN_SERVER` 時，btn 0 / btn 2 切換至 `SCREEN_DASHBOARD`
- [ ] 4.7 確認 `SCREEN_SERVER` 不在 `s_nav_screens[]` 中

## 5. 驗證

- [ ] 5.1 完整 build 無錯誤（`./flash.sh --build-only`）
- [ ] 5.2 Flash 測試：WiFi 已連線 → 中間鍵進 Server screen → QR code 顯示正確 IP
- [ ] 5.3 Flash 測試：WiFi 未連線 → Server screen 顯示提示訊息，不啟動 HTTP server
- [ ] 5.4 Flash 測試：瀏覽器輸入 IP 可開啟設定頁面，各功能（WiFi/AI/股票）正常
- [ ] 5.5 Flash 測試：中間鍵三點輪巡（Dashboard → Portal → Server → Dashboard）
- [ ] 5.6 Flash 測試：在 Server screen 按左/右鍵回 Dashboard
- [ ] 5.7 Flash 測試：Portal 啟動後進 Server screen，再離開，refcount 正確（HTTP server 不意外停止）
