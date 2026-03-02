## 1. 常數與 Storage API 聲明

- [x] 1.1 在 `include/app_config.h` 的 WiFi Portal 區段後新增 `WIFI_MAX_AP_COUNT 5`、`WIFI_SSID_MAX_LEN 33`、`WIFI_PASS_MAX_LEN 65` 三個常數
- [x] 1.2 在 `components/storage/include/storage.h` 保留舊 API，新增 `storage_wifi_ap_count()`、`storage_wifi_save_ap()`、`storage_wifi_load_ap()`、`storage_wifi_remove_ap()`、`storage_wifi_add_ap()`、`storage_wifi_migrate_legacy()` 六個函數聲明

## 2. Storage 實作

- [x] 2.1 在 `components/storage/storage.c` 新增 `make_ap_ssid_key()` 與 `make_ap_pass_key()` 兩個輔助函數（`snprintf(buf, 16, "ap_%d_ssid/pass", idx)`）
- [x] 2.2 實作 `storage_wifi_ap_count()`：開 READONLY handle，讀 `ap_count`，找不到回傳 0
- [x] 2.3 實作 `storage_wifi_save_ap()`：開 READWRITE handle，寫 ssid/pass key，commit
- [x] 2.4 實作 `storage_wifi_load_ap()`：開 READONLY handle，讀 ssid/pass key
- [x] 2.5 實作 `storage_wifi_remove_ap()`：slot shifting（往前覆蓋）→ 清除末尾 key → 更新 `ap_count`，單次 commit
- [x] 2.6 實作 `storage_wifi_add_ap()`：掃描重複 SSID → upsert；若滿則先呼叫 `remove_ap(0)` → 寫入末尾，單次 commit
- [x] 2.7 實作 `storage_wifi_migrate_legacy()`：檢查 `ap_count` 是否存在 → 已遷移則回傳；讀舊 `ssid/password` → 寫入 slot 0 + 設 `ap_count=1` + 刪舊 key，單次 commit；無舊資料則設 `ap_count=0`

## 3. main.c 遷移呼叫

- [x] 3.1 在 `main/main.c` 的 `ESP_ERROR_CHECK(storage_init())` 後一行插入 `storage_wifi_migrate_legacy()`

## 4. WiFi Manager API 與連線邏輯

- [x] 4.1 在 `components/wifi_manager/include/wifi_manager.h` 新增 `esp_err_t wifi_manager_connect_any_saved(void)` 聲明
- [x] 4.2 在 `components/wifi_manager/wifi_manager.c` 於 `wifi_manager_connect_saved()` 前實作 `wifi_manager_connect_any_saved()`：遍歷已儲存 AP 依序呼叫 `wifi_manager_connect()`，第一個成功回傳 ESP_OK；全部失敗呼叫 `notify_state(WIFI_STATE_FAILED)` 後回傳 ESP_FAIL；count=0 呼叫 `notify_state(WIFI_STATE_DISCONNECTED)` 回傳 ESP_ERR_NOT_FOUND
- [x] 4.3 修改 `wifi_manager_connect_saved()` 改為直接 `return wifi_manager_connect_any_saved()`
- [x] 4.4 修改 `wifi_manager_connect()` 成功後的 `storage_wifi_save(ssid, password)` 改為 `storage_wifi_add_ap(ssid, password)`

## 5. Portal 新增 HTTP Handlers

- [x] 5.1 在 `wifi_manager.c` 新增 `portal_saved_aps_get_handler`（`GET /saved_aps`）：遍歷 `storage_wifi_ap_count()` 個 AP，組成 JSON 陣列 `[{"ssid":"..."}]`（不含密碼），256 bytes stack buffer
- [x] 5.2 在 `wifi_manager.c` 新增 `portal_saved_aps_remove_post_handler`（`POST /saved_aps/remove`）：解析 form-urlencoded `ssid` 欄位 → 遍歷找到匹配後呼叫 `storage_wifi_remove_ap(idx)` → 回傳 `{"ok":true}`；找不到回 404 `{"ok":false,"error":"not_found"}`
- [x] 5.3 在 `start_portal_http_server()` 中新增兩個 `httpd_uri_t` 結構並呼叫 `httpd_register_uri_handler()` 註冊

## 6. Portal WiFi Post Handler 補充

- [x] 6.1 在 `portal_wifi_post_handler` 的 `free(body)` 後、建立連線 task 前，插入空密碼自動填入邏輯：若 `conn_req->password[0] == '\0'` 且 SSID 不為空，遍歷已儲存 AP 找到匹配 SSID 後 `strncpy` 對應密碼至 `conn_req->password`

## 7. Portal HTML 與前端 JS 更新

- [x] 7.1 在 WiFi tab 的 `</form>` 後、`</div>`（card 結尾）前插入 `<h3>Saved Networks</h3><div id='saved_aps_list'>Loading...</div>`
- [x] 7.2 在 `<script>` 區塊頂端新增全域變數 `var s_saved_ssids=[];`
- [x] 7.3 在 fetch('/scan') 的 `a.forEach` 中加入 `[Saved]` badge：`var saved=s_saved_ssids.indexOf(x.ssid)>=0;` 並在 `o.textContent` 中拼接 `(saved?' [Saved]':'')`
- [x] 7.4 新增 JS 函數 `loadSavedAps()`：fetch `/saved_aps` → 更新 `s_saved_ssids` → 渲染 `saved_aps_list`，每筆含 Remove 按鈕（`onclick="removeSavedAp('...')"`）；無資料顯示 "No saved networks"
- [x] 7.5 新增 JS 函數 `removeSavedAp(ssid)`：POST `/saved_aps/remove`（application/x-www-form-urlencoded）成功後呼叫 `loadSavedAps()`
- [x] 7.6 修改 `showTab()` 函數：在 `if(tab==='stocks'){loadStocks();}` 後新增 `if(tab==='wifi'){loadSavedAps();}`

## 8. 驗證

- [x] 8.1 執行 `./flash.sh --build-only` 確認 build 無錯誤
- [x] 8.2 執行 `./flash.sh --erase` 清除 NVS 後燒錄，確認 monitor log 顯示 `ap_count=0 初始化`
- [x] 8.3 連上 `Core2-Setup` AP → 開啟 `http://192.168.4.1` → WiFi tab 顯示「Saved Networks」區塊（初始為空）
- [x] 8.4 連線至一個 AP → 確認 Saved Networks 出現該 SSID
- [x] 8.5 再連線另一個 AP → Saved Networks 顯示兩筆；掃描結果中兩個 SSID 顯示 `[Saved]` badge
- [x] 8.6 點「Remove」移除一筆 → 清單即時更新
- [x] 8.7 燒錄舊版本（含單筆 WiFi 設定）後升版，確認 monitor log 顯示「WiFi 設定已從舊格式遷移」，裝置自動連線
- [x] 8.8 重啟裝置，確認 log 顯示依序嘗試已儲存 AP 的訊息
