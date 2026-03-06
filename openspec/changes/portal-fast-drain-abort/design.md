## Context

進入 Portal screen 時需先 drain STA 網路活動（停 telegram bot + 暫停 quote fetch），等待 WiFi driver internal buffer 釋放後才能安全啟動 SoftAP。

為什麼 drain 不能移除：ESP32 只有一顆 WiFi radio，AP 和 STA 是 time-sharing 同一顆 radio，WiFi driver 的 internal DRAM buffer pool 也是共用的（與通用 heap 含 PSRAM 是不同記憶體池，Resource screen 看不到）。STA 有 active traffic 時啟動 SoftAP，WiFi driver 無法為 AP 分配足夠 buffer，導致 `ieee80211_hostap_attach` NULL pointer crash。因此 drain 是必要的，問題在於 drain 太慢。

目前 drain 是被動等待 HTTP 請求自然結束或超時，最壞情況需等 `HTTP_TIMEOUT_MS + 5000 = 20s`。使用者體驗差。

根本原因：`telegram_bot_stop()` 只設 flag，不中斷 in-flight HTTP 請求；`scheduler_pause_quote_polling()` 同理。

## Goals / Non-Goals

**Goals:**
- 將 Portal drain 等待從最壞 20s 降至 < 1s
- 透過主動 abort in-flight HTTP client 實現快速 drain
- 確保 abort 後無記憶體洩漏

**Non-Goals:**
- 變更 Portal 啟動流程或 APSTA 架構
- 變更正常（非 drain 場景）的 HTTP timeout 設定
- 變更 drain 的非致命行為（timeout 後仍啟動 portal）

## Decisions

### D1：主動 abort 策略

在 `telegram_bot_stop()` 和 quote fetch pause 時，主動關閉 in-flight 的 `esp_http_client` handle，使 `esp_http_client_perform()` 立即返回 error。

### D2：Thread safety 方案（待確認）

`esp_http_client_close()` 跨 task 呼叫的安全性需先驗證：

| 方案 | 說明 | 風險 |
|------|------|------|
| A: `esp_http_client_close()` | 直接 close handle | ESP-IDF 未明確保證 thread-safe |
| B: socket `shutdown()` | 關底層 socket fd，讓 perform 收到 error | 繞過 HTTP client 抽象層 |
| C: abort flag + 短 timeout | 不主動 close，改用更短的 HTTP timeout（如 2s）搭配 abort flag | 最保守但仍需等 2s |

需在實作前用測試確認方案 A 的可行性。若不可行則退回 B 或 C。

## Scope

### 需修改的檔案

| 檔案 | 修改內容 |
|------|---------|
| `components/telegram_bot/telegram_bot.c` | stop() 時主動 abort HTTP client |
| `components/twse_client/twse_client.c` | fetch 被 pause 時主動 abort HTTP client |
| `components/scheduler/scheduler.c` | pause 時通知 fetch abort（若 fetch handle 在 scheduler 管理） |
| `components/ui/ui_manager.c` | 縮短 `PORTAL_NET_DRAIN_TIMEOUT_MS`（drain 變快後 timeout 可大幅縮短） |

### 不修改的部分

- Portal 啟動流程（`portal_backend_start`）
- WiFi mode 切換邏輯
- 正常 HTTP 請求的 timeout 設定

## Risks

| 情境 | 風險 | 緩解 |
|------|------|------|
| `esp_http_client_close()` 跨 task 不安全 | use-after-free 或 crash | 先用方案 A 測試，不行退回 B/C |
| abort 後 HTTP client 資源未正確釋放 | 記憶體洩漏 | 確認 error path 有 cleanup |
| abort 時 cJSON parse 正在處理部分 response | parse error | abort 應在 perform 層，不影響 parse（perform 未返回就不會 parse） |

## Verification

1. `idf.py build` 編譯通過 (`m5stack-core2-flash`)
2. 燒錄後按中鍵進入 Portal，觀察進入速度（目標 < 2s）(`m5stack-core2-flash`)
3. 在 quote fetch / telegram polling 進行中按中鍵，確認 portal 正常啟動 (`m5stack-core2-flash`)
4. 反覆進出 Portal 5 次，確認無記憶體洩漏（Resource screen heap 穩定）
5. 離開 Portal 後 telegram 和 quote fetch 正常恢復
