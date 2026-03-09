## Why

WiFi 未連線時，系統行為不合理：telegram bot 無條件啟動浪費 DRAM、scheduler task 阻塞式等待 WiFi、runtime WiFi 斷線後畫面停在 dashboard 顯示過時報價且無自動復原路徑。需要 WiFi 狀態驅動的生命週期管理，讓裝置在無網路時自動進入 portal 配網模式、停止無用 task，並在 WiFi 恢復時自動回到 dashboard。

## What Changes

- WiFi 斷線（retry 耗盡）時自動切換到 SCREEN_PORTAL，啟動 AP portal 供配網
- WiFi 連線成功時自動從 SCREEN_PORTAL 切回 SCREEN_DASHBOARD，恢復所有服務
- Boot 時若無 WiFi，不啟動 telegram bot（延後到 WiFi 連線後才 start）
- Scheduler task 初始化從阻塞式 busy-wait 改為 notify-based 非阻塞等待，等待期間仍可處理 cmd queue
- Scheduler timer 在無 WiFi 時不啟動（paused 狀態），由 portal exit 時 resume
- 自動導航透過 main loop 消費 volatile flag 實現（避免 event bus dispatch task 4KB stack 不足）

## Capabilities

### New Capabilities

- `wifi-lifecycle-nav`: WiFi 狀態驅動的自動頁面切換（FAILED → portal、CONNECTED → dashboard）與 task 生命週期管理（bot 延後啟動、scheduler 條件啟動 timer）

### Modified Capabilities

- `scheduler`: scheduler task 初始化從阻塞式 WiFi 等待改為 notify-based 非阻塞等待；timer 在無 WiFi 時不啟動
- `main`: boot flow 延後 telegram_bot_start；main loop 增加 pending nav 消費
- `telegram-bot-control`: bot 啟動時機從無條件改為 WiFi 連線後才啟動

## Impact

| 範圍 | 影響 |
|------|------|
| `main/main.c` | boot flow 重排、WiFi callback 增加自動導航邏輯、main loop 增加 nav 消費 |
| `components/scheduler_service/core.c` | 移除阻塞 WiFi 等待、條件啟動 timer、新增 notify API |
| `components/scheduler_service/include/internal.h` | 新增 WIFI_UP notify bit |
| `components/scheduler_service/include/scheduler_service.h` | 新增 `scheduler_service_notify_wifi_connected()` API |
| DRAM | 無 WiFi 時不啟動 bot task，節省 ~8KB internal DRAM |
| UX | 無需手動切換頁面，WiFi 斷線/恢復自動切換 portal/dashboard |
