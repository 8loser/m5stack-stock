## Why

目前 AtTime 只輸出到 ESP log，使用者在裝置上的 Log 頁無法直接看到定時任務是否執行成功或失敗。當 AI 呼叫或 Telegram 發送失敗時，現場排障成本高。

同時 Core2 資源有限、Log 頁是小螢幕（320x240），需要以低成本方式補足可觀測性，避免新增額外 queue/event 或過長文案造成 ring buffer 快速洗掉。

## What Changes

- 在 AtTime 執行路徑補上 log screen 記錄，成功與失敗都要有紀錄
- 新增 AtTime 專用 log tag，畫面顯示文字為「定時」
- 採短碼文案（例如 `AT#2 AI_OK`、`AT#2 TG_FAIL:ESP_FAIL`）降低換行與佔用
- 失敗採「分階段」記錄，但僅記錄真正執行失敗（不記暫時 skip）
- 維持現有 Log 頁刷新策略（切頁時刷新），不新增常駐即時刷新機制

## Capabilities

### Modified Capabilities

- `scheduler`: AtTime 觸發流程可將執行結果寫入 UI log
- `ui-manager`: 新增 AtTime log wrapper，統一 log push 入口
- `log-page`: 支援 AtTime tag 與「定時」顯示

## Impact

- `components/scheduler_service/at_time_worker.c` -- 修改：AtTime 成功/失敗分階段短碼記錄
- `components/ui/include/screen_log.h`、`components/ui/screens/screen_log.c` -- 修改：新增 `LOG_TAG_AT` 與顯示字串「定時」
- `components/ui/include/ui_manager.h`、`components/ui/ui_manager.c` -- 修改：新增 `ui_manager_log_at()`
