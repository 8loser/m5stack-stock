## 1. Device server callback contract

- [x] 1.1 在 `device_server.h` 新增 `stock_list_changed_cb_t` 型別與 `device_server_set_stock_list_changed_callback(...)` API。
- [x] 1.2 在 `device_server.c` 新增 callback 儲存與 setter 實作，保持與既有 WiFi callback 並存。

## 2. Portal stock handlers integration

- [x] 2.1 在 `/stocks/add` 成功寫入 NVS 後觸發 stock-list-changed callback，傳入 `list.count`。
- [x] 2.2 在 `/stocks/remove` 成功寫入 NVS 後觸發 stock-list-changed callback，傳入 `list.count`。
- [x] 2.3 確保所有 add/remove 失敗路徑不觸發 callback。

## 3. Main bridge to UI

- [x] 3.1 在 `main.c` 新增 `on_stock_list_changed(uint8_t count)`，橋接呼叫 `ui_manager_set_dashboard_card_count(count)`。
- [x] 3.2 在 `device_server_init()` 後註冊 stock-list-changed callback，並保留既有 WiFi callback 註冊流程。

## 4. Validation

- [x] 4.1 執行 `./flash.sh --build-only` 確認編譯通過。
- [ ] 4.2 手動驗證：當股票總數 > 5 時，Portal add 成功後切回 dashboard 仍維持固定 5 卡槽，且輪巡內容可包含新股票。
- [ ] 4.3 手動驗證：當股票總數 > 5 時，Portal remove 成功後切回 dashboard 仍維持固定 5 卡槽，且輪巡內容不再出現被移除股票。
- [ ] 4.4 手動驗證：add/remove 失敗案例不影響 dashboard 目前輪巡內容與卡槽顯示。
