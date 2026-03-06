## Why

進入 Portal screen 前的 drain 等待最壞需 20 秒（HTTP_TIMEOUT_MS + 5s）。drain 是必要的（釋放 WiFi driver internal buffer 讓 SoftAP 啟動），但被動等待 HTTP 請求自然結束太慢，使用者體驗差。

## What Changes

- `telegram_bot_stop()` 和 quote fetch pause 時，主動 abort in-flight HTTP client，使 drain 從被動等待（~20s）變為主動中斷（<1s）
- 縮短 `PORTAL_NET_DRAIN_TIMEOUT_MS`（drain 變快後不需要長 timeout）

## Capabilities

### Modified Capabilities

- `telegram-bot`: stop() 時主動 abort HTTP client handle
- `twse-client` / `scheduler`: pause 時主動 abort fetch 中的 HTTP client handle
- `ui-manager`: 縮短 drain timeout

## Impact

- `components/telegram_bot/telegram_bot.c` -- 修改：stop() 主動 close HTTP client
- `components/twse_client/twse_client.c` -- 修改：支援外部 abort fetch
- `components/scheduler/scheduler.c` -- 修改：pause 時通知 fetch abort
- `components/ui/ui_manager.c` -- 修改：縮短 PORTAL_NET_DRAIN_TIMEOUT_MS
