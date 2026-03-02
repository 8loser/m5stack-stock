## 1. 前置設定

- [ ] 1.1 在 `main/app_config.h` 新增 `DASHBOARD_PAGE_FLIP_S` 常數（預設值 5）

## 2. Scheduler API

- [ ] 2.1 在 `components/scheduler/scheduler.h` 宣告 `uint32_t scheduler_get_seconds_to_next_quote(void)`
- [ ] 2.2 在 `components/scheduler/scheduler.c` 實作：使用 `xTimerGetExpiryTime` 與 `xTaskGetTickCount` 計算剩餘秒數，timer 為 NULL 時回傳 0

## 3. Dashboard — card 容器重構

- [ ] 3.1 移除 `screen_dashboard.c` 中 `s_cards[]` 直接掛在 `s_screen` 的做法，改為新增 `s_card_list` 捲動容器（透明背景、無 border、pad_all=0，pos=(4,24), size=(LCD_WIDTH-8, 182)）
- [ ] 3.2 將 `MAX_CARDS` 改為使用 `MAX_STOCK_COUNT`，card 建立迴圈上限改為 `MAX_STOCK_COUNT`
- [ ] 3.3 每張 card 改為掛在 `s_card_list`，位置改為 `(0, i * 36)`，寬度改為 `LCD_WIDTH - 8`
- [ ] 3.4 每張 card 建立時加入 `lv_obj_set_style_pad_all(s_cards[i], 0, 0)` 並預設 `LV_OBJ_FLAG_HIDDEN`

## 4. Dashboard — 空白提示與 footer 標籤

- [ ] 4.1 在 `s_card_list` 內建立 `s_empty_label`，文字為 `"No stocks configured.\nGo to Settings to add."`，`LV_ALIGN_CENTER`，預設 hidden
- [ ] 4.2 將現有 `s_update_label` 位置調整至 `(4, 207)`
- [ ] 4.3 新增 `s_next_label`，pos=(4, 220)，font=montserrat_10，初始文字 `"Next: --:--:--"`，顏色 0x888888

## 5. Dashboard — 動態 card 數與自動翻頁

- [ ] 5.1 新增 `screen_dashboard_set_card_count(uint8_t n)` 函式：顯示前 `min(n, MAX_STOCK_COUNT)` 張 card、隱藏其餘；n=0 時顯示 `s_empty_label`，n≥1 時隱藏；重設 `s_cur_page=0` 並 scroll_to_y(0)
- [ ] 5.2 新增靜態變數 `s_cur_page` 與 LVGL Timer（`lv_timer_create`，週期 `DASHBOARD_PAGE_FLIP_S * 1000` ms）
- [ ] 5.3 實作 page flip timer callback：計算 `total_pages = ceil(s_card_count / 5.0)`，推進 `s_cur_page`，呼叫 `lv_obj_scroll_to_y(s_card_list, s_cur_page * 5 * 36, LV_ANIM_ON)`；`s_card_count <= 5` 時無操作

## 6. Dashboard — next time 更新

- [ ] 6.1 在 `apply_card_widgets()` 末尾新增：呼叫 `scheduler_get_seconds_to_next_quote()`，換算為絕對時間字串，更新 `s_next_label` 文字為 `"Next: HH:MM:SS"`
- [ ] 6.2 在 `screen_dashboard.c` 加入 `#include "scheduler.h"`

## 7. UI Manager

- [ ] 7.1 在 `components/ui/ui_manager.h` 宣告 `void ui_manager_set_dashboard_card_count(uint8_t n)`
- [ ] 7.2 在 `components/ui/ui_manager.c` 實作：持 `g_ui_mutex` 後呼叫 `screen_dashboard_set_card_count(n)`

## 8. main.c 串接

- [ ] 8.1 在 `storage_init()` 之後讀取 `stock_list_t`，呼叫 `ui_manager_set_dashboard_card_count(list.count)`
