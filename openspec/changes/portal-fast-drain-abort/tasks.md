## 0. 前置調查

- [ ] 0.1 確認 `esp_http_client_close()` 跨 task 呼叫是否安全（查 ESP-IDF 原始碼或測試）
- [ ] 0.2 根據 0.1 結果決定 abort 策略：A（直接 close）/ B（socket shutdown）/ C（短 timeout + flag）

## 1. telegram_bot -- 主動 abort

- [ ] 1.1 將 `esp_http_client_handle_t` 提升為模組級變數（或受保護的 shared state）
- [ ] 1.2 在 `telegram_bot_stop()` 中主動 abort HTTP client（依 0.2 決定的策略）
- [ ] 1.3 確認 telegram_task 的 error path 正確 cleanup（無記憶體洩漏）

## 2. twse_client / scheduler -- 主動 abort fetch

- [ ] 2.1 將 fetch 用的 `esp_http_client_handle_t` 提升為可外部存取
- [ ] 2.2 在 `scheduler_pause_quote_polling()` 時主動 abort fetch HTTP client
- [ ] 2.3 確認 fetch error path 正確 cleanup

## 3. ui_manager -- 縮短 timeout

- [ ] 3.1 將 `PORTAL_NET_DRAIN_TIMEOUT_MS` 從 `HTTP_TIMEOUT_MS + 5000` 縮短為 2000~3000ms（主動 abort 後只需短暫等待 task 退出）

## 4. 驗證 (m5stack-core2-flash)

- [ ] 4.1 `idf.py build` 編譯通過
- [ ] 4.2 燒錄後按中鍵進入 Portal，進入速度 < 2s
- [ ] 4.3 在 quote fetch / telegram polling 進行中按中鍵，確認 portal 正常啟動
- [ ] 4.4 反覆進出 Portal 5 次，Resource screen 確認 heap 穩定（無洩漏）
- [ ] 4.5 離開 Portal 後 telegram 和 quote fetch 正常恢復
