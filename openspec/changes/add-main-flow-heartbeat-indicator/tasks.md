## 1. UI Manager 心跳核心

- [ ] 1.1 在 `ui_manager.h` 新增主流程心跳 API 宣告（main/scheduler feed 與 alive 查詢）。
- [ ] 1.2 在 `ui_manager.c` 新增心跳時間戳狀態、門檻常數與 thread-safe 更新邏輯。
- [ ] 1.3 實作 `ui_manager_is_main_flow_alive()` 回傳 alive 與 age 資訊，供 UI 元件讀取。

## 2. Status Bar 視覺指標

- [ ] 2.1 在 `status_bar.c` 新增心跳 label，固定放在時間前方並納入布局。
- [ ] 2.2 實作雙擊節奏動畫狀態機（正常態）與 `!` 停閃（異常態）。
- [ ] 2.3 將心跳渲染與既有 10 秒狀態更新解耦（新增較高頻 timer 或等效機制）。

## 3. 主流程餵心跳接線

- [ ] 3.1 在 `main/main.c` 主迴圈每輪呼叫 `ui_manager_heartbeat_feed_main()`。
- [ ] 3.2 在 `scheduler_task` 每輪呼叫 `ui_manager_heartbeat_feed_scheduler()`。
- [ ] 3.3 將 `scheduler_task` 的 `xTaskNotifyWait` timeout 改為 1000ms，保留原通知語意。

## 4. Emoji 字型與 fallback

- [ ] 4.1 在 `tools/fonts/ui_symbols.txt` 加入心跳 emoji 字元。
- [ ] 4.2 確認 `generate_fonts.sh` 產生流程可將新字元編入 `lv_font_noto_tc_14/16`。
- [ ] 4.3 在 `status_bar.c` 加入 glyph fallback（emoji 不可用時顯示 `<3`）。

## 5. 驗證

- [ ] 5.1 執行 `./flash.sh --build-only`，確認編譯通過。
- [ ] 5.2 實機確認各 screen 都可見心跳，且正常時為雙擊節奏。
- [ ] 5.3 注入主流程阻塞情境，確認逾時後切為 `!` 並停閃。
- [ ] 5.4 `./flash.sh --monitor` 檢查無新增 LVGL 警告與異常 log。
