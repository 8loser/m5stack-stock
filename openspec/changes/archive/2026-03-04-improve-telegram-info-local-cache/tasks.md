## 1. Telegram quote local cache

- [x] 1.1 在 `components/telegram_bot/include/telegram_bot.h` 新增 `telegram_bot_cache_quote(const stock_quote_t *quote)` API
- [x] 1.2 在 `components/telegram_bot/telegram_bot.c` 新增 RAM 快取結構（quotes + updated_at + last_update）與 mutex 保護
- [x] 1.3 實作 symbol 對應更新與查詢 helper，支援 stale 門檻判定（1800 秒）

## 2. `/info` response refactor

- [x] 2.1 移除 `/info` 路徑中的 `twse_client_fetch()` 與即時抓價 fallback 邏輯
- [x] 2.2 改為讀取 `storage_stocks_load` + `storage_stock_meta_load` + RAM quote 快取組字串
- [x] 2.3 統一輸出格式：`symbol` 在前，含名稱、價格/N/A、更新時間與 `(stale)` 標記
- [x] 2.4 保留既有 `/help` 與未知指令回覆行為

## 3. Main loop integration

- [x] 3.1 在 `main/main.c` queue 消費 quote 後呼叫 `telegram_bot_cache_quote(&quote)`
- [x] 3.2 確認 UI 更新與 Telegram 快取更新互不影響（不改既有 UI mutex 使用方式）

## 4. Startup backlog discard

- [x] 4.1 在 telegram task 進入常駐 polling 前新增 bootstrap sync 流程
- [x] 4.2 解析 bootstrap `getUpdates` 最大 `update_id` 並設定 `s_next_update_id = max + 1`
- [x] 4.3 bootstrap sync 失敗時延後重試，不執行任何命令

## 5. Verification

- [x] 5.1 執行 `./flash.sh --build-only`，確認編譯通過
- [x] 5.2 實機驗證：`/info` 回覆期間不觸發 TWSE fetch
- [x] 5.3 實機驗證：關機期間累積訊息，重開後舊訊息不執行
- [x] 5.4 實機驗證：快取缺資料顯示 N/A；超過 30 分鐘顯示 `(stale)`
