## Context

AtTime 在 `scheduler_service_check_at_time()` 命中後進入 `fire_at_time_entry()`，流程包含：
1. 組 prompt（global + entry）
2. 呼叫 AI provider
3. 成功後發送 Telegram

目前只有 `ESP_LOG*`，Log 頁 ring buffer（32 筆）沒有 AtTime 專屬紀錄。使用者要求成功、失敗都要在 Log 頁可見，且在小螢幕上要易讀、資源開銷低。

## Goals / Non-Goals

**Goals:**
- AtTime 每次實際觸發都有可觀測紀錄（成功/失敗）
- 不新增 event bus type、不新增 queue，沿用既有 log ring buffer
- 文案在 320x240 上維持高可讀性與低換行率

**Non-Goals:**
- 不做 log 持久化
- 不新增 Log 頁即時刷新 loop
- 不記錄非執行失敗（如 disabled、weekday 不符、quote in-flight skip）

## Decisions

### D1：接入點使用 `fire_at_time_entry()` 直寫 UI log

在 AtTime 真正執行路徑直接呼叫 `ui_manager_log_at()`，避免新增 app_event_bus payload 與 dispatch 成本。

### D2：新增 AT tag，畫面顯示「定時」

- enum 新增 `LOG_TAG_AT`
- `log_tag_to_str()` 顯示字串新增「定時」
- INFO 顏色為新 tag 專屬色（沿用現有 WARN/ERROR 覆蓋規則）

### D3：分階段短碼紀錄

每次觸發採短碼，格式固定 `AT#<idx> <STATE>`，其中 `idx` 為 entry index。

- 成功路徑：
  - `AT#N AI_OK`
  - `AT#N TG_OK`
- 失敗路徑：
  - `AT#N OOM`
  - `AT#N AI_FAIL:<ERR>`
  - `AT#N AI_EMPTY`
  - `AT#N TG_FAIL:<ERR>`

### D4：失敗範圍限制為真正執行失敗

不記錄暫時略過事件（例如 quote fetch in-flight），避免同分鐘反覆刷屏，保留 ring buffer 對關鍵錯誤的承載能力。

## Risks

| 風險 | 影響 | 緩解 |
|------|------|------|
| 分階段紀錄使單次觸發產生 2 筆以上 log | ring buffer 較快覆寫 | 使用短碼、限制只記真正失敗 |
| 錯誤碼文字過長造成換行 | 小螢幕可讀性下降 | 訊息上限由現有 log 截斷機制保護 |
| 新增 tag 後顏色辨識不清 | 快速掃讀變差 | 使用與既有 tag 可區分的 INFO 色 |

## Verification

1. `./flash.sh --build-only` 編譯通過（`m5stack-core2-dev`）
2. 設定一筆可成功 AtTime，確認 Log 頁出現 `AI_OK` + `TG_OK`
3. 模擬 AI 失敗，確認 Log 頁出現 `AI_FAIL:<ERR>`
4. 模擬 Telegram 失敗，確認 Log 頁出現 `TG_FAIL:<ERR>`
5. 確認 Log 頁 tag 顯示含「定時」，既有「股票/WiFi/系統」不回歸
