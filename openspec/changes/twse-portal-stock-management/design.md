## Context

此 change 在 `e50f6ba`、`8b8a2c8` commit 中已大部分實作。現有狀態：

| 項目 | 狀態 |
|------|------|
| `twse_client_validate_symbol()` | ✅ 已實作（twse_client.c:182）|
| `stock_symbol_info_t` | ✅ 已定義（twse_client.h）|
| `scheduler_reload_stock_list()` | ✅ 已實作（scheduler.c:253）|
| `GET /stocks` handler | ✅ 已實作並註冊 |
| `POST /stocks/add` handler | ✅ 已實作並註冊（wifi_manager.c:517）|
| `POST /stocks/remove` handler | ✅ 已實作並註冊（wifi_manager.c:582）|
| Portal 前端 Stocks tab | ✅ 已實作（tab-based HTML UI）|

## Goals / Non-Goals

**Goals:**
- 透過 Portal 網頁管理股票觀察清單（查詢 / 新增 / 刪除）
- 新增前以 TWSE API 驗證代號存在且為上市（market=tse）
- 資料變更後呼叫 `scheduler_reload_stock_list()` 下個排程週期生效
- 僅支援 TWSE 上市股（4 位數字，market=tse）

**Non-Goals:**
- 即時觸發報價抓取（資料變更後不立即抓價）
- 支援 TPEx 上櫃（`otc_XXXX.tw`）
- 排序或批次操作

## Decisions

### D1：股票清單 helper 實作為 wifi_manager.c 私有靜態函數

**原 proposal**：在 `storage.h/c` 新增 `storage_stocks_add/remove/has()` helper。

**實際做法**：`stock_list_find_symbol()`、`stock_list_add_symbol()`、`stock_list_remove_symbol()` 實作為 `wifi_manager.c` 內的 `static` 函數。`storage.h/c` 維持不變，仍使用 `storage_stocks_load()` + `storage_stocks_save()` 作為底層持久化。

**理由**：這些 helper 只在 HTTP handler 場景中使用，沒有其他元件需要存取。放在 `wifi_manager.c` 避免擴大 storage 公開 API，降低耦合。

### D2：驗證順序

```
POST /stocks/add 驗證流程：
  1. 格式驗證：4 位數字（is_symbol_format_valid）
  2. 查重：stock_list_find_symbol
  3. 上限：count >= MAX_STOCK_COUNT（10 檔）
  4. WiFi 連線：is_sta_connected（離線無法呼叫 TWSE API）
  5. TWSE API 驗證：twse_client_validate_symbol
  6. market 欄位驗證：info.market == "tse"（排除 OTC）
```

### D3：股票名稱 In-memory Cache

`GET /stocks` 需要在清單中顯示股票名稱（name / abbr），但 NVS 的 `stock_list_t` 只儲存 symbol。實作一個 `s_stock_meta_cache[]` 陣列（`MAX_STOCK_COUNT` 筆），在 `POST /stocks/add` 成功後呼叫 `cache_stock_meta(symbol, name, abbr)` 填入。重開機後 cache 為空，`GET /stocks` 在啟動時觸發一次 TWSE API 驗證補全 cache（`portal_stocks_get_handler` 初始化路徑）。

### D4：前端 Tab-based UI

Portal HTML 採用三個 tab 切換：WiFi / AI Provider / Stocks。Tab 以 JavaScript 控制 `display:none/block`，無頁面重新載入。Stocks tab 含：
- `loadStocks()` 呼叫 `GET /stocks`，動態渲染股票清單
- 每列顯示 symbol + name + Remove 按鈕
- 底部輸入框 + Add 按鈕，呼叫 `POST /stocks/add`

### D5：HTTP 回應格式

成功回應：
```json
// GET /stocks
{"ok": true, "items": [{"symbol":"2330","name":"台積電","abbr":"TSMC"}]}

// POST /stocks/add
{"ok": true, "item": {"symbol":"2330","name":"台積電","abbr":"TSMC","market":"tse"}}

// POST /stocks/remove
{"ok": true}
```

錯誤回應：`{"ok": false, "error": "<error_code>"}`

錯誤碼：`invalid_format`、`duplicate_symbol`、`limit_exceeded`、`validate_failed`、`not_found_or_not_tse`

### D6：TWSE API 驗證端點

`twse_client_validate_symbol()` 呼叫：
```
GET https://mis.twse.com.tw/stock/api/getStockInfo.jsp?ex_ch=tse_<symbol>.tw&json=1&delay=0
```

解析 `msgArray[0]` 的 `c`（symbol）、`n`（abbr）、`nf`（full name）、`ex`（market）欄位。`msgArray` 為空或 item 不存在代表代號不合法（`info.exists = false`）。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| 重開機後 meta cache 為空 | `GET /stocks` handler 啟動時對現有 symbol 補呼叫 TWSE API 填 cache |
| TWSE API 驗證逾時（10s）導致 HTTP 請求等待 | timeout 設 `HTTP_TIMEOUT_MS`；Portal 前端應有等待 indicator（目前未實作）|
| `is_sta_connected()` 偶發誤判 | 使用 `esp_wifi_sta_get_ap_info()` 直接查 AP 連線狀態，比 wifi_manager state cache 更即時 |
| 多個 HTTP handler 並行呼叫 `stock_list_*` helper | httpd 預設單執行緒；多 handler 不會並行，無競爭問題 |

## Migration Plan

此 change 已實作完成，無需進一步實作步驟。

待確認事項：
1. 重開機後 meta cache 補全邏輯是否正確運作
2. WiFi 離線時 `POST /stocks/add` 是否正確回傳 502
3. Portal 前端在 Add 過程中是否有適當的 loading 狀態
