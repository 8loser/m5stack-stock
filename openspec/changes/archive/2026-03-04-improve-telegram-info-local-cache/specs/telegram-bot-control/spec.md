## ADDED Requirements

### Requirement: `/info` SHALL use local device data only
系統在處理 Telegram `/info` 指令時 MUST 僅使用 Core2 本機資料（股票清單、股票 meta、RAM 最後報價快取），且 MUST NOT 在 `/info` 命令路徑呼叫 TWSE API。

#### Scenario: `/info` with cached quote
- **WHEN** 使用者送出 `/info` 且某股票存在本機快取報價
- **THEN** 回覆內容顯示 `symbol + name + price/change` 與該筆快取的最後更新時間

#### Scenario: `/info` without cached quote
- **WHEN** 使用者送出 `/info` 且某股票尚無本機快取報價
- **THEN** 該股票行顯示 `N/A`，且回覆不觸發任何 TWSE 請求

### Requirement: `/info` SHALL include freshness marker for stale cache
系統 MUST 以固定 30 分鐘門檻判定快取新鮮度。超過門檻的股票資料在 `/info` 回覆中 MUST 顯示 `(stale)` 標記。

#### Scenario: stale cache marker
- **WHEN** 股票快取時間距離目前時間超過 30 分鐘
- **THEN** `/info` 中該股票資料行尾包含 `(stale)`

### Requirement: Bot startup SHALL discard pre-boot backlog
Telegram bot 啟動時 MUST 先同步最新 `update_id`，並將 polling offset 設為最新值 + 1。啟動前累積的 Telegram 訊息 MUST 被忽略且 MUST NOT 觸發指令執行。

#### Scenario: messages sent while device is offline
- **WHEN** 裝置關機期間 chat 內累積多筆 `/info` 訊息，且裝置重新開機
- **THEN** bot 啟動後不回覆這些舊訊息，只處理開機後新收到的訊息

#### Scenario: bootstrap sync transient failure
- **WHEN** bot 啟動 bootstrap sync 發生 HTTP 或 JSON 解析失敗
- **THEN** bot 延後重試並暫不進入命令處理，避免誤處理 backlog
