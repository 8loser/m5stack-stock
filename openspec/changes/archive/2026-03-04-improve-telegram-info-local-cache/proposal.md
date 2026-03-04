## Why

目前 Telegram `/info` 會在指令當下即時呼叫 TWSE，造成回覆結果依賴外網與即時 API；同時 bot 重開機後可能處理離線期間累積的舊訊息，導致過時指令被執行。需要把 `/info` 改為純本機資料回覆，並在啟動時丟棄舊更新，確保行為可預期。

## What Changes

- 調整 `/info` 回覆來源：不再於指令路徑呼叫 `twse_client_fetch()`
- 新增 Telegram 模組內 RAM quote 快取 API，供主迴圈在收到排程報價時更新
- `/info` Stocks 行改為讀取本機資料（stocks/meta + RAM 快取），並顯示最後更新時間與 stale 標記
- Telegram task 啟動時先同步最新 `update_id` 並跳過 backlog，不執行開機前累積訊息
- 既有 `/help` 與未知指令回覆行為維持不變

## Capabilities

### New Capabilities
- `telegram-bot-control`: 定義 Telegram `/info` 本機資料回覆與啟動時舊訊息丟棄規則

### Modified Capabilities
- （無）

## Impact

- `components/telegram_bot/include/telegram_bot.h`：新增 quote 快取 API 宣告
- `components/telegram_bot/telegram_bot.c`：實作 RAM 快取、`/info` 本機輸出、啟動 backlog 丟棄
- `main/main.c`：在 queue 消費報價後同步更新 Telegram 快取
- 行為影響：`/info` 不再觸發 TWSE request；開機前 Telegram 指令不再被執行
