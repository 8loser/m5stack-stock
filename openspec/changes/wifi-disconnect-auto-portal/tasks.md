## 1. Scheduler Service: 非阻塞 WiFi 等待 + 條件啟動

- [ ] 1.1 在 `internal.h` 新增 `SCHEDULER_SERVICE_NOTIFY_WIFI_UP_BIT (1U << 3)`
- [ ] 1.2 在 `scheduler_service.h` 新增 `void scheduler_service_notify_wifi_connected(void)` 宣告
- [ ] 1.3 在 `core.c` 實作 `scheduler_service_notify_wifi_connected()`：guard check `s_ctx.scheduler_task != NULL` 後 `xTaskNotify` 送出 WIFI_UP bit
- [ ] 1.4 在 `core.c` 改寫 `scheduler_service_task` 初始化段：將 busy-wait 改為 `xTaskNotifyWait` 迴圈，處理 CMD_BIT 和 WIFI_UP_BIT，維持心跳發布
- [ ] 1.5 在 `core.c` 改寫 `scheduler_service_init()`：WiFi 已連線時 `xTimerStart`；WiFi 未連線時設 `quote_polling_paused = true` 且不啟動 timer

## 2. Main: Boot Flow 重排 + 自動導航

- [ ] 2.1 在 `main.c` 新增 `static volatile int s_pending_nav = -1`
- [ ] 2.2 將 `telegram_bot_start()` 從無條件呼叫改為 WiFi 連線成功後才呼叫
- [ ] 2.3 擴充 `on_wifi_state()` callback：CONNECTED 時呼叫 `scheduler_service_notify_wifi_connected()` + 設 pending nav；FAILED 時設 pending nav to PORTAL
- [ ] 2.4 在 main loop 中新增 pending nav 消費邏輯：DASHBOARD 目標先 delay 500ms + re-check，PORTAL 目標直接切換

## 3. 驗證

- [ ] 3.1 編譯通過（`./flash.sh build`）
- [ ] 3.2 燒錄實測：boot 無 WiFi → 自動進入 portal、bot 未啟動、scheduler timer 未啟動
- [ ] 3.3 燒錄實測：portal 配網成功 → 自動切回 dashboard、scheduler resume + force fetch、bot 啟動
- [ ] 3.4 燒錄實測：runtime WiFi 斷線 → 5 次 retry 後自動切 portal、bot stop、scheduler pause
