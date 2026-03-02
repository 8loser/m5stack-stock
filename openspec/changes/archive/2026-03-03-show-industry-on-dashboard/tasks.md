## 1. TWSE metadata and models

- [x] 1.1 擴充 `stock_symbol_info_t` 增加 `industry` 欄位，並更新相關 header 註解。
- [x] 1.2 在 `twse_client_validate_symbol()` 增加產業別解析邏輯（缺值時回空字串，不影響 exists/market 判定）。
- [x] 1.3 視需要新增 metadata 查詢 helper，統一 name/abbr/industry 的來源與 fallback。

## 2. Storage and portal API integration

- [x] 2.1 在 `storage` 新增股票 metadata 持久化 API（save/load/remove，包含 industry）。
- [x] 2.2 調整 `device_server` stock meta cache 結構，加入 `industry` 並更新 `cache_stock_meta()`/`clear_stock_meta()`。
- [x] 2.3 調整 `POST /stocks/add`：新增成功後寫入 metadata（含 industry），回傳 JSON 增加 `industry`。
- [x] 2.4 調整 `GET /stocks`：回傳每筆 `industry`；缺值時回空字串且不在此路徑補抓。
- [x] 2.5 調整 `POST /stocks/remove`：刪除 symbol 時同步移除 metadata。

## 3. Dashboard data flow and UI layout

- [x] 3.1 擴充 Dashboard 使用資料模型（`stock_quote_t` 或等價 UI DTO）帶入 `industry`。
- [x] 3.2 在 quote→UI 路徑補齊 `industry` 資料映射，確保 UI 不直接查網路。
- [x] 3.3 修改 `screen_dashboard.c` 卡片布局為四欄：`symbol+name | industry | price | change%`。
- [x] 3.4 為第二欄加入長字串截斷策略，確認不壓縮價格與漲跌欄位。

## 4. Validation

- [x] 4.1 執行 `./flash.sh --build-only`，確認編譯通過。
- [x] 4.2 手動驗證：新增股票成功時 Dashboard 第二欄顯示產業別。
- [x] 4.3 手動驗證：產業別缺值時 Dashboard 顯示空字串或未知值且不破版。
- [x] 4.4 手動驗證：移除股票後 metadata 清除，重開機後列表與 Dashboard 狀態一致。
