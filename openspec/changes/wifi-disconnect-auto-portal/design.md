## Context

目前系統各模組（telegram bot、scheduler、UI）的生命週期在 boot 時無條件啟動，WiFi 狀態變化不會觸發任何 task 停啟或頁面切換。Scheduler task 以阻塞式 busy-wait 等待 WiFi，無法處理 cmd queue。Runtime WiFi 斷線後裝置停在 dashboard 顯示過時報價，使用者需手動切到 portal 頁面。

## Goals / Non-Goals

**Goals:**
- WiFi 狀態變化自動驅動頁面切換（FAILED → portal、CONNECTED → dashboard）
- 無 WiFi 時不啟動 telegram bot，節省 ~8KB internal DRAM
- Scheduler 初始化改非阻塞，等待期間仍處理 cmd queue 與心跳
- Scheduler timer 在無 WiFi 時不啟動，由 portal exit resume
- 複用既有 `ui_manager_switch_screen` 的 portal entry/exit 編排，不新建生命週期管理邏輯

**Non-Goals:**
- WiFi auto-reconnect 策略變更（wifi_manager 既有 5 次 retry 不變）
- Portal UI 變更
- 新增 event bus 事件類型
- APSTA 模式（Core2 DRAM 不足，已確認不可用）

## Decisions

### D1: 自動導航透過 main loop volatile flag 實現

**選擇**: `on_wifi_state()` callback 設定 `static volatile int s_pending_nav`，main loop 每 20ms 消費。

**替代方案**: 透過 event bus dispatch task 直接呼叫 `ui_manager_switch_screen`。

**理由**: dispatch task 只有 4KB stack，`ui_manager_switch_screen` 進入 portal 時執行 `portal_backend_start()`（AP netif 建立、HTTPD 啟動）等重量級操作，stack 不足。Main loop 在 app_main task（ESP-IDF 預設 8KB），可安全執行。20ms 延遲對使用者體感無影響。

### D2: 觸發自動切 portal 的狀態為 WIFI_STATE_FAILED

**選擇**: 只在 `WIFI_STATE_FAILED`（retry 全部耗盡）時觸發，不在每次 `WIFI_STATE_DISCONNECTED` 時觸發。

**理由**: WiFi 斷線後 wifi_manager 自動 retry 5 次，期間會發多次 DISCONNECTED 事件。只在 FAILED（確認無法恢復）才切 portal，避免瞬斷造成不必要的頁面跳轉和 task 停啟。

### D3: WiFi 連線成功自動切回 dashboard 前加 500ms 延遲

**選擇**: `s_pending_nav = SCREEN_DASHBOARD` 後，main loop 消費時先 `vTaskDelay(500ms)` 再切頁。

**理由**: WiFi 從 portal 配網成功時，portal HTTPD 可能還在送 HTTP response。延遲 500ms 讓 response 完成，再關閉 HTTPD。

### D4: 延後 telegram_bot_start 到 WiFi 連線後

**選擇**: `telegram_bot_init()` 仍無條件呼叫（只分配狀態），`telegram_bot_start()` 移到 WiFi connect 成功之後。

**理由**: bot task 佔 8KB internal DRAM。無 WiFi 時 bot 無法 polling，白佔 DRAM。`telegram_bot_stop()` 對未啟動的 bot 是 safe no-op（只設 flag），portal entry 的 stop 邏輯不受影響。

### D5: Scheduler notify-based WiFi 等待

**選擇**: 新增 `SCHEDULER_SERVICE_NOTIFY_WIFI_UP_BIT (1<<3)` 和 `scheduler_service_notify_wifi_connected()` API。Task init 段改為 `xTaskNotifyWait` 迴圈，同時處理 cmd queue 和心跳。

**理由**: 既有阻塞式 `while(!wifi)` 無法處理 cmd queue（如 AI test key），心跳間隔也不穩定。Notify-based 等待保持 1s 粒度，且可被 WiFi 連線事件立即喚醒。

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| WiFi flapping 造成頻繁頁面切換 | 只在 FAILED（5 次 retry 全部失敗）才觸發；`s_pending_nav` last-write-wins 消除中間態 |
| Portal HTTP response 被截斷（WiFi 連線成功 → HTTPD 關閉） | 500ms 延遲 + 切頁前再次確認 current screen |
| `on_wifi_state` callback 在 boot 期間觸發（`network_portal_connect_saved` 同步呼叫） | `ui_manager_is_startup_guard_active()` 守衛，boot 期間忽略自動導航 |
| Main loop 20ms 延遲 | 人類感知門檻 ~100ms，20ms 完全無感 |
| Scheduler timer 在無 WiFi 時未啟動，portal exit resume 前 timer 不會 fire | 這是期望行為；resume 會立即 force fetch |
