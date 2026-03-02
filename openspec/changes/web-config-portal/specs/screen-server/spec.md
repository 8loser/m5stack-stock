## ADDED Requirements

### Requirement: Server screen 顯示 STA IP QR code
當 Core2 已連上 WiFi（STA 模式有效 IP），`screen_server` SHALL 產生並顯示以 `http://<STA_IP>` 為內容的 QR code，並以文字形式顯示完整 URL。

#### Scenario: WiFi 已連線進入 Server screen
- **WHEN** 使用者進入 Server screen 且 STA IP 不為 0.0.0.0
- **THEN** 畫面顯示 QR code（內容為 `http://A.B.C.D`）與 URL 文字標籤

#### Scenario: WiFi 未連線進入 Server screen
- **WHEN** 使用者進入 Server screen 且 STA IP 為 0.0.0.0（未連線）
- **THEN** 畫面顯示提示訊息「Not connected to WiFi」，不顯示 QR code，不啟動 web server

### Requirement: Server screen load 時啟動 web server
`screen_server` SHALL 在 load 時（進入頁面）呼叫 `device_server_start_web_server()`，使 HTTP server 開始接受連線。

#### Scenario: 進入 Server screen 啟動 HTTP server
- **WHEN** 使用者切換至 Server screen 且 WiFi 已連線
- **THEN** `device_server_start_web_server()` 被呼叫，HTTP server 開始在 port 80 監聽

#### Scenario: WiFi 未連線不啟動 HTTP server
- **WHEN** 使用者切換至 Server screen 且 WiFi 未連線
- **THEN** `device_server_start_web_server()` 不被呼叫

### Requirement: Server screen unload 時停止 web server
`screen_server` SHALL 在 unload 時（離開頁面）呼叫 `device_server_stop_web_server()`，釋放 HTTP server 資源。

#### Scenario: 離開 Server screen 停止 HTTP server
- **WHEN** 使用者從 Server screen 切換至其他頁面
- **THEN** `device_server_stop_web_server()` 被呼叫

### Requirement: IP 在進入頁面時即時取得
Server screen SHALL 在每次 load 時即時呼叫 `esp_netif_get_ip_info()` 取得當前 STA IP，不快取前次結果。

#### Scenario: 重新進入 Server screen 更新 IP
- **WHEN** 使用者離開後再次進入 Server screen
- **THEN** 畫面顯示當前最新的 STA IP，而非上次進入時的 IP
