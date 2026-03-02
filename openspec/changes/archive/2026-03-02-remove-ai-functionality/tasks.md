## 1. 移除 AI 設定欄位與排程 API 依賴

- [x] 1.1 在 `main/app_config.h` 的 `schedule_config_t` 移除 `ai_interval_min` 欄位
- [x] 1.2 在 `components/storage/storage.c` 的 schedule NVS 讀寫邏輯移除 `ai_ivl` key 存取，僅保留 quote interval 與 market-hours-only
- [x] 1.3 在 `components/scheduler/include/scheduler.h` 將 `scheduler_init()` 簽名改為只接受 `quote_queue`
- [x] 1.4 在 `components/scheduler/scheduler.c` 移除 AI timer 初始化、AI 週期任務與 `scheduler_trigger_ai_now()` 實作

## 2. 移除主流程 AI 執行路徑

- [x] 2.1 在 `main/main.c` 移除 `g_ai_result_queue` 宣告與建立
- [x] 2.2 在 `main/main.c` 更新 `scheduler_init(...)` 呼叫為只傳入 quote queue
- [x] 2.3 在 `main/main.c` 移除 AI queue 消費與 `ui_manager_log_ai()` 呼叫區塊
- [x] 2.4 在 `main/main.c` 移除或停用 `ai_provider` 初始化與設定載入流程

## 3. 更新 UI 模組與 Info 頁內容

- [x] 3.1 在 `components/ui_manager/include/ui_manager.h` 移除 `ui_manager_log_ai()` 宣告
- [x] 3.2 在 `components/ui_manager/ui_manager.c` 移除 `ui_manager_log_ai()` 實作
- [x] 3.3 在 `components/screen_info/screen_info.c` 移除 AI section 與對應 label，保留 Device/Network/Stocks 三個 section
- [x] 3.4 在 `components/screen_info/screen_info.c` 的 Stocks 區塊顯示 `Quote: <Xs>  Market-only: <Y/N>`（不含 AI interval）與股票清單（空清單顯示 `None`）

## 4. 保留 Portal AI tab 儲存能力但移除執行觸發

- [x] 4.1 在 `components/wifi_manager/wifi_manager.c` 保留 AI tab 的 GET/POST handler 與 NVS 儲存行為
- [x] 4.2 在 `components/wifi_manager/wifi_manager.c` 的 AI POST handler 移除 `ai_provider_reload_local_config` 或任何 AI 執行觸發呼叫
- [x] 4.3 檢查 Portal 導覽維持 `WiFi`、`AI`、`Stocks` 三個 tab，且 AI tab 僅做 API key 儲存

## 5. 從 build 系統移除 ai_provider 元件

- [x] 5.1 在根目錄 `CMakeLists.txt` 的 `EXTRA_COMPONENT_DIRS` 移除 `components/ai_provider`
- [x] 5.2 在各相依元件的 `CMakeLists.txt`/`idf_component_register` 內移除 `ai_provider` 的 `REQUIRES` 依賴
- [x] 5.3 確認 `components/ai_provider/` 目錄與內容仍保留在檔案系統中

## 6. 編譯與行為驗證

- [x] 6.1 執行 `./flash.sh --build-only`，確認編譯成功且無 AI 相關連結錯誤
- [x] 6.2 進入 portal 驗證 WiFi/AI/Stocks 三個 tab 皆可用，AI tab 可儲存 key
- [x] 6.3 切換到 Info 頁面驗證三個 section 可捲動，Stocks 區塊顯示新格式內容
- [x] 6.4 檢查執行期 log，確認不再出現 AI timer 觸發或 AI HTTP 分析請求
