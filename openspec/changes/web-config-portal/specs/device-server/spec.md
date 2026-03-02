## ADDED Requirements

### Requirement: 元件由 wifi_manager 改名為 device_server
原 `wifi_manager` 元件（目錄、標頭、所有公開 API）SHALL 改名為 `device_server`，所有 `wifi_manager_*` 函式前綴改為 `device_server_*`。

#### Scenario: 改名後編譯成功
- **WHEN** 所有引用 `wifi_manager` 的檔案更新為 `device_server`
- **THEN** 專案完整編譯無 undefined reference 或 missing header 錯誤

### Requirement: 新增獨立 web server 啟停 API
`device_server` SHALL 提供 `device_server_start_web_server()` 與 `device_server_stop_web_server()` 兩個公開函式，在不啟動 SoftAP 的情況下獨立啟動或停止 HTTP server。

#### Scenario: STA 模式下啟動 web server
- **WHEN** `device_server_start_web_server()` 被呼叫且裝置處於 STA 模式
- **THEN** HTTP server 在 port 80 啟動，回傳 `ESP_OK`

#### Scenario: web server 已在運行時重複呼叫
- **WHEN** `device_server_start_web_server()` 被呼叫時 HTTP server 已在運行（refcount > 0）
- **THEN** refcount 遞增，回傳 `ESP_OK`，不重新初始化 server

#### Scenario: 停止 web server 但 portal 仍在運行
- **WHEN** `device_server_stop_web_server()` 被呼叫但 provisioning portal 仍啟用（refcount > 1）
- **THEN** refcount 遞減，HTTP server 繼續運行，不停止

#### Scenario: refcount 歸零時停止 HTTP server
- **WHEN** `device_server_stop_web_server()` 被呼叫且呼叫後 refcount 為 0
- **THEN** HTTP server 停止，`s_httpd` 設為 NULL

### Requirement: provisioning portal 共用 HTTP server
`device_server_start_provisioning_portal()` SHALL 在啟動 SoftAP 的同時呼叫 `device_server_start_web_server()`；`device_server_stop_provisioning_portal()` SHALL 呼叫 `device_server_stop_web_server()`。兩者共用同一份 HTTP server 程式碼與 HTML 頁面。

#### Scenario: 啟動 provisioning portal 同時啟動 HTTP server
- **WHEN** `device_server_start_provisioning_portal()` 成功啟動 SoftAP
- **THEN** `device_server_start_web_server()` 被呼叫，HTTP server 在 192.168.4.1:80 可訪問

#### Scenario: 停止 provisioning portal 同時釋放 HTTP server
- **WHEN** `device_server_stop_provisioning_portal()` 被呼叫
- **THEN** `device_server_stop_web_server()` 被呼叫，refcount 遞減
