## MODIFIED Requirements

### Requirement: Bot startup SHALL discard pre-boot backlog
Telegram bot 啟動時 MUST 先同步最新 `update_id`，並將 polling offset 設為最新值 + 1。啟動前累積的 Telegram 訊息 MUST 被忽略且 MUST NOT 觸發指令執行。

Bot 啟動時機 SHALL 延後到 WiFi 連線成功後。Boot 時若無 WiFi，bot MUST NOT 被啟動（`telegram_bot_start()` 不被呼叫）。Bot 在首次 WiFi 連線成功後由 portal exit 邏輯或 boot flow 啟動。

#### Scenario: messages sent while device is offline
- **WHEN** 裝置關機期間 chat 內累積多筆 `/info` 訊息，且裝置重新開機
- **THEN** bot 啟動後不回覆這些舊訊息，只處理開機後新收到的訊息

#### Scenario: bootstrap sync transient failure
- **WHEN** bot 啟動 bootstrap sync 發生 HTTP 或 JSON 解析失敗
- **THEN** bot 延後重試並暫不進入命令處理，避免誤處理 backlog

#### Scenario: Boot 無 WiFi 時 bot 不啟動
- **WHEN** 裝置開機且 WiFi 連線失敗
- **THEN** `telegram_bot_start()` 不被呼叫，bot task 不存在，節省 ~8KB internal DRAM

#### Scenario: WiFi 從 portal 連線成功後 bot 啟動
- **WHEN** 使用者透過 portal 配網成功，UI 自動切回 dashboard
- **THEN** portal exit 邏輯呼叫 `telegram_bot_start()`（AP mode path），bot 正常啟動並執行 bootstrap sync
