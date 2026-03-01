## 1. screen_log.h — 公開 API 宣告

- [x] 1.1 建立 `components/ui/include/screen_log.h`，宣告 `log_tag_t` enum（LOG_TAG_STOCK / WIFI / AI / SYS）
- [x] 1.2 宣告 `log_level_t` enum（LOG_LEVEL_INFO / WARN / ERROR）
- [x] 1.3 宣告 `screen_log_push(log_tag_t, log_level_t, const char *msg)`
- [x] 1.4 宣告 `lv_obj_t *screen_log_create(void)` 與 `void screen_log_refresh(void)`

## 2. screen_log.c — Ring Buffer 與顯示

- [x] 2.1 定義 `log_entry_t` struct（tag / level / msg[56]）與 32 條靜態 ring buffer
- [x] 2.2 實作 `screen_log_push()`：portMUX 保護寫入，訊息截斷至 55 字元
- [x] 2.3 重寫 `screen_log_create()`：建立背景、標題 label，以及 12 個靜態條目 label（y = 28 + i*17，montserrat_12）
- [x] 2.4 實作 `screen_log_refresh()`：從最新往最舊迭代 ring buffer，更新 12 個 label 文字與顏色
- [x] 2.5 實作顏色對應函數：INFO 依 tag 選色，WARN 強制 0xFFB74D，ERROR 強制 0xEF5350

## 3. ui_manager.c — screen_log_refresh 整合

- [x] 3.1 在 `ui_manager_switch_screen()` 的 mutex 持有段內，新增 `if (id == SCREEN_LOG)` 分支呼叫 `screen_log_refresh()`

## 4. ui_manager — Log Wrapper API

- [x] 4.1 在 `ui_manager.h` 加入 `screen_log.h` 的 include 並宣告 4 個 wrapper：`ui_manager_log_stock/wifi/ai/sys(log_level_t, const char *fmt, ...)`
- [x] 4.2 在 `ui_manager.c` 實作 4 個 wrapper：`vsnprintf` 格式化後呼叫對應 tag 的 `screen_log_push()`

## 5. main.c — 事件 Log 整合

- [x] 5.1 在 `on_wifi_state()` 新增 log 呼叫：CONNECTED → `ui_manager_log_wifi(INFO, "Connected: %s", ip)`；其他狀態 → `ui_manager_log_wifi(WARN, "Disconnected")`
- [x] 5.2 在 `app_main()` 初始化完成後（進入主迴圈前）呼叫 `ui_manager_log_sys(INFO, "System ready")`
- [x] 5.3 改寫主迴圈 quote drain：drain 完後計算筆數 N，若 N ≥ 1 則呼叫 `ui_manager_log_stock(INFO, "Updated %d stock(s)", N)`
- [x] 5.4 主迴圈新增非阻塞 `xQueueReceive(g_ai_result_queue, &ai_result, 0)`，收到後呼叫 `ui_manager_log_ai(INFO, "AI: %.50s", ai_result.text)`

## 6. 驗證

- [x] 6.1 切換至 Log 頁面，確認 label 更新（最新條目在最上方）
- [x] 6.2 WiFi 連線 / 斷線，切換至 Log 頁面確認 WIFI 條目顏色正確（INFO=綠、WARN=橙）
- [x] 6.3 等待 scheduler 觸發報價更新，確認 Log 出現 `Updated N stock(s)` 摘要而非多筆
- [x] 6.4 確認超過 32 條時最舊條目被覆蓋，頁面可捲動瀏覽事件並保持最新在頂部
