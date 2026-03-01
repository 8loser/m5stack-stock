## 1. twse_client — validate_symbol（已實作）

- [x] 1.1 在 `twse_client.h` 宣告 `stock_symbol_info_t` 與 `twse_client_validate_symbol()`
- [x] 1.2 在 `twse_client.c` 實作 `twse_client_validate_symbol()`：呼叫 TWSE API，解析 msgArray[0] 填入 out

## 2. scheduler — 熱重載（已實作）

- [x] 2.1 在 `scheduler.h` 宣告 `scheduler_reload_stock_list()`
- [x] 2.2 在 `scheduler.c` 實作 `scheduler_reload_stock_list()`：重新從 NVS 載入清單，下個週期生效

## 3. wifi_manager — 股票 HTTP API（已實作）

- [x] 3.1 實作 `GET /stocks` handler：載入 NVS 清單，補全 meta cache，回傳 JSON 陣列
- [x] 3.2 實作靜態 helper：`is_symbol_format_valid`、`stock_list_find/add/remove_symbol`
- [x] 3.3 實作 `POST /stocks/add` handler：依序格式→查重→上限→連線→TWSE驗證→市場→儲存→reload
- [x] 3.4 實作 `POST /stocks/remove` handler：查找→移除→儲存→reload
- [x] 3.5 實作 `cache_stock_meta()` / `find_stock_meta()` / `clear_stock_meta()` in-memory cache
- [x] 3.6 在 `start_provisioning_portal()` 中註冊三個新路由

## 4. Portal 前端 HTML — Stocks tab（已實作）

- [x] 4.1 在 Portal HTML 加入 Stocks tab 按鈕（tab-based UI）
- [x] 4.2 實作 `loadStocks()` 呼叫 `GET /stocks`，動態渲染清單與 Remove 按鈕
- [x] 4.3 實作 Add 按鈕呼叫 `POST /stocks/add`，成功後重新呼叫 `loadStocks()`

## 5. 驗證（待確認）

- [ ] 5.1 新增合法上市股票（如 2330），確認清單出現且下個排程週期抓到新股價
- [ ] 5.2 新增不存在代號，確認回傳 404 not_found_or_not_tse
- [ ] 5.3 新增上櫃代號，確認回傳 404（market != tse）
- [ ] 5.4 重複新增同一代號，確認回傳 409 duplicate_symbol
- [ ] 5.5 新增第 11 筆，確認回傳 409 limit_exceeded
- [ ] 5.6 WiFi 離線時新增，確認回傳 502 validate_failed
- [ ] 5.7 刪除股票，確認清單消失且下個排程週期不再抓該股票
- [ ] 5.8 重開機後，確認 `GET /stocks` 仍能顯示名稱（meta cache 補全邏輯）
