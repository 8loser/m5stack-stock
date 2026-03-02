## Context

`wifi_manager` 目前同時負責 WiFi STA/AP 管理與 HTTP provisioning server（包含完整 HTML 設定頁、WiFi 連線、AI 設定、股票管理等 endpoint）。HTTP server 以 `static` 函式封裝，與 SoftAP 啟停強綁定，無法獨立使用。

本次變更將元件正式改名為 `device_server`，並將 HTTP server 啟停從 SoftAP 解耦，使 `screen_server`（STA 模式）和 `screen_portal`（SoftAP 模式）共用同一份 HTTP server 程式碼。

受影響檔案：8 個（`wifi_manager.*` × 3、`screen_portal.c`、`screen_info.c`、`ui/CMakeLists.txt`、`main/CMakeLists.txt`、`main.c`）。

## Goals / Non-Goals

**Goals:**
- 將 `wifi_manager` 元件改名為 `device_server`，所有 API 前綴同步更新
- 新增 `device_server_start_web_server()` / `device_server_stop_web_server()` 公開 API
- HTTP server 可在無 SoftAP 的情況下（STA 模式）獨立啟動
- 新增 `screen_server`：顯示 STA IP QR code，load/unload 時啟停 web server
- 中間鍵改為三點輪巡：Dashboard → Portal → Server → Dashboard

**Non-Goals:**
- 不修改 HTML 設定頁內容或任何 HTTP endpoint 邏輯
- 不更動 SoftAP provisioning 流程
- 不新增 NVS key 或分區
- 不實作 HTTPS 或認證機制

## Decisions

### 1. HTTP server 啟停參考計數

**問題**：`s_httpd` 為單一 handle，Portal（SoftAP）和 Server screen（STA）都可能各自呼叫啟停。若 Portal 啟動中，Server screen 的 `stop_web_server()` 不應停掉 HTTP server。

**決策**：引入 `s_web_server_refcount`（整數）。每次 `start_web_server` +1，`stop_web_server` -1，只有 refcount 歸零才真正停止 `httpd`。`start_provisioning_portal()` 和 `start_web_server()` 各自 +1；`stop_provisioning_portal()` 和 `stop_web_server()` 各自 -1。

**替代方案**：用 bool flag 判斷誰啟動了 server，但多個來源同時存在時難以處理。參考計數更通用。

### 2. `screen_server` 取得 STA IP

**決策**：直接呼叫 `esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info)`，取得 `ip_info.ip`，格式化為 `http://A.B.C.D`。不透過 `device_server` 封裝，保持簡單。

若 IP 為 `0.0.0.0`（未連線），顯示提示訊息「Not connected to WiFi」，不啟動 web server。

### 3. 中間鍵三點輪巡實作

**決策**：在 `ui_manager.c` 新增 `static int s_mid_btn_idx = 0`，按下中間鍵時依序切換：

```
static const screen_id_t s_mid_screens[] = {
    SCREEN_DASHBOARD, SCREEN_PORTAL, SCREEN_SERVER
};
```

`s_mid_btn_idx = (s_mid_btn_idx + 1) % 3`，切換至 `s_mid_screens[s_mid_btn_idx]`。

在 Portal 或 Server 頁面時，左右鍵行為不變（仍操作 `s_nav_screens[]`，但因 Portal/Server 不在其中，實際上不切換）。

**替代方案**：改為「中間鍵從任意頁面直接跳 Portal/Server，再按回 Dashboard」，但用戶明確指定三點輪巡，維持一致性。

### 4. 元件改名策略

**決策**：直接改名（目錄、檔案、所有 symbol），不保留舊名稱相容層。受影響檔案數量少（8 個），且都在同一 repo 內，直接全量替換風險可控。

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| Portal 啟動中進入 Server screen，refcount 正確但 SoftAP IP（192.168.4.1）與 STA IP 共存 | 兩個 QR 指向不同 IP，功能正確；文件說明即可 |
| WiFi 斷線後 Server screen 仍顯示舊 IP QR | screen_server 進入時即時取 IP，不快取；重新進入頁面即更新 |
| 改名過程遺漏引用導致編譯失敗 | tasks 中明確列出每個需要修改的檔案 |

## Migration Plan

1. 重新命名 `components/wifi_manager/` → `components/device_server/`，同步改所有檔名與 symbol
2. 新增 `device_server_start_web_server()` / `device_server_stop_web_server()` + refcount 邏輯
3. 新增 `screen_server.c` 與 `SCREEN_SERVER`
4. 更新 `ui_manager.c` 中間鍵邏輯
5. 全量 build 驗證，flash 測試三個情境：STA 已連線進 Server screen、SoftAP Portal、兩者依序切換

無需 rollback 策略（純 refactor + 新功能，不改資料格式）。

## Open Questions

無。
