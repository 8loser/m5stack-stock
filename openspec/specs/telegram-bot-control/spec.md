# telegram-bot-control Specification

## Purpose
TBD - created by syncing change improve-telegram-info-local-cache. Update Purpose after archive.

## Requirements
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

### Requirement: stock alert trigger SHALL send summary + AI response
當 stock alert 門檻觸發且 AI 呼叫成功時，系統 MUST 發送 Telegram 訊息，內容包含：
- 觸發摘要（symbol、方向、門檻、現價、漲跌幅）
- AI 回覆全文

#### Scenario: stock alert success message
- **WHEN** 某股票觸發門檻且 AI 分析成功
- **THEN** Telegram 收到一則包含觸發摘要與 AI 分析結果的訊息

### Requirement: stock alert AI failure SHALL send one failure summary
當 stock alert 已觸發但 AI 呼叫失敗（例如 key 缺失、HTTP/解析失敗），系統 MUST 發送一則失敗摘要通知，且同一事件 MUST NOT 立即重試第二次 AI 呼叫。

#### Scenario: missing provider key
- **WHEN** 觸發 stock alert 但目前 primary provider 沒有可用 key
- **THEN** 發送失敗摘要通知，不做 provider fallback

#### Scenario: AI call failed
- **WHEN** 觸發 stock alert 且 AI 呼叫返回錯誤
- **THEN** 發送失敗摘要通知，不做即時重試
