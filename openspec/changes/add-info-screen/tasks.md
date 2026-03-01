## 1. screen_info.c — 頁面骨架與容器

- [ ] 1.1 在 `screen_info_create()` 內建立可捲動 container（y=30, height=190, 垂直捲動）
- [ ] 1.2 預配置 4 個 section header label（顏色 0x4FC3F7，montserrat_12）：DEVICE / NETWORK / AI / STOCKS
- [ ] 1.3 預配置 4 個 content label（白色，montserrat_12，`LV_LABEL_LONG_WRAP`）
- [ ] 1.4 宣告 8 個靜態 `lv_obj_t *` 指標供 `screen_info_refresh()` 存取

## 2. screen_info.c — screen_info_refresh() 實作

- [ ] 2.1 實作 Device 資料讀取：`esp_get_free_heap_size()`、`esp_chip_info()`、`esp_get_idf_version()`，以 `\n` 合併為單一字串更新 content_lbl_0
- [ ] 2.2 實作 Network 資料讀取：`storage_wifi_load()` 讀 SSID（空則 "Not saved"）、`wifi_manager_get_state()` 轉文字、`wifi_manager_get_ip()` 含 NULL check（顯示 "--"），更新 content_lbl_1
- [ ] 2.3 實作 AI 資料讀取：`ai_provider_get_name()`，`ai_provider_get_active_api_key()` 並遮蔽（末 4 碼顯示，空則 "Not configured"），更新 content_lbl_2
- [ ] 2.4 實作 Stocks 資料讀取：`storage_stocks_load()` + `storage_schedule_load()`，組合排程參數與 symbol 清單（空則 "None"），更新 content_lbl_3

## 3. ui_manager.c — switch_screen 整合

- [ ] 3.1 在 `ui_manager_switch_screen()` 的 mutex 持有段新增 `if (id == SCREEN_INFO)` 分支，呼叫 `screen_info_refresh()`

## 4. CMakeLists.txt 依賴確認

- [ ] 4.1 確認 `components/ui/CMakeLists.txt` 的 REQUIRES 含 `esp_system`；若無則補上

## 5. 驗證

- [ ] 5.1 切換至 Info 頁面，確認 Device section 顯示 heap、chip、IDF 版本
- [ ] 5.2 確認 Network section 在 WiFi 已連線時顯示正確 IP，未連線時顯示 "--"
- [ ] 5.3 確認 AI section 顯示 provider 名稱，API key 正確遮蔽
- [ ] 5.4 確認 Stocks section 顯示股票代號清單與排程參數
- [ ] 5.5 確認內容超出可視範圍時可向下滑動查看完整 Stocks section
