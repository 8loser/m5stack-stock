## Why

Portal 目前只提供 WiFi 設定與 AI 設定，使用者無法在不重新燒錄的情況下管理股票觀察清單。需要在 Portal 新增股票清單管理介面，並加入 TWSE 即時驗證以確保代號合法。

## What Changes

- 新增三個 HTTP API：`GET /stocks`（查詢清單）、`POST /stocks/add`（新增，含 TWSE 驗證）、`POST /stocks/remove`（刪除）
- Portal 前端新增 Stocks 卡片：顯示清單（symbol + name）、輸入框新增、每列 Remove 按鈕
- 新增前驗證順序：格式（4 位數字）→ 查重 → 上限（10 檔）→ TWSE API 驗證（market=tse）
- 資料變更後採下個排程週期生效（不立即抓價）
- 僅支援 TWSE（上市），不接 TPEx

## Capabilities

### New Capabilities

- `portal-stock-management`: Portal 股票清單管理 UI，支援查詢、新增（含 TWSE 驗證）、刪除

### Modified Capabilities

- `twse-client`: 新增 `stock_symbol_info_t` 型別與 `twse_client_validate_symbol()` API
- `storage`: 新增股票清單查重 / 加入 / 刪除 helper
- `scheduler`: 新增 `scheduler_reload_stock_list()` 熱重載 API（不立即觸發抓價）
- `wifi-manager`: 新增 `/stocks`、`/stocks/add`、`/stocks/remove` 路由與 handlers

## Impact

- `components/twse_client/include/twse_client.h` — 修改：新增 `stock_symbol_info_t`、`twse_client_validate_symbol()` 宣告
- `components/twse_client/twse_client.c` — 修改：實作 `twse_client_validate_symbol()`
- `components/storage/include/storage.h` — 修改：新增查重 / 加入 / 刪除 helper 宣告
- `components/storage/storage.c` — 修改：實作三個 helper
- `components/scheduler/include/scheduler.h` — 修改：新增 `scheduler_reload_stock_list()` 宣告
- `components/scheduler/scheduler.c` — 修改：實作 `scheduler_reload_stock_list()`
- `components/wifi_manager/wifi_manager.c` — 修改：註冊三個新路由、實作 handlers、統一 JSON 回應格式
- `components/wifi_manager/portal/index.html`（或內嵌 HTML）— 修改：新增 Stocks 卡片 UI 與前端邏輯
