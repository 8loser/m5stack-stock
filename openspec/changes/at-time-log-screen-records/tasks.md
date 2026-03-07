## 1. log-page / ui-manager 介面擴充

- [ ] 1.1 在 `screen_log.h` 新增 `LOG_TAG_AT`
- [ ] 1.2 在 `screen_log.c` 補上 `LOG_TAG_AT` 顯示字串「定時」與 INFO 顏色映射
- [ ] 1.3 在 `ui_manager.h/.c` 新增 `ui_manager_log_at(log_level_t level, const char *fmt, ...)`

## 2. scheduler AtTime 執行結果記錄

- [ ] 2.1 在 `fire_at_time_entry()` 成功/失敗路徑加入短碼 log
- [ ] 2.2 失敗僅覆蓋真正執行失敗：OOM / AI fail / AI empty / Telegram fail
- [ ] 2.3 不新增 skip 類事件（例如 quote in-flight）的 log

## 3. 驗證

- [ ] 3.1 `./flash.sh --build-only` 通過
- [ ] 3.2 AtTime 成功路徑可看到 `AT#N AI_OK` 與 `AT#N TG_OK`
- [ ] 3.3 AI 失敗路徑可看到 `AT#N AI_FAIL:<ERR>`
- [ ] 3.4 Telegram 失敗路徑可看到 `AT#N TG_FAIL:<ERR>`
- [ ] 3.5 Log 頁可見新 tag「定時」，其他 tag 顯示不回歸
