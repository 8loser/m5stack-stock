## 1. ui_manager 整合

- [x] 1.1 在 `screen_id_t` 加入 `SCREEN_SETTINGS = 4`
- [x] 1.2 加入 `screen_settings_create()` / `screen_settings_load()` 宣告
- [x] 1.3 `ui_manager_init()` 建立 `s_screens[SCREEN_SETTINGS]`
- [x] 1.4 `ui_manager_switch_screen()` 切到 Settings 前呼叫 `screen_settings_load()`
- [x] 1.5 `s_nav_screens[]` 納入 `SCREEN_SETTINGS`，可用硬體按鍵輪詢切換

## 2. settings 頁面

- [x] 2.1 新增 `components/ui/screens/screen_settings.c`
- [x] 2.2 版面改為三個 interval 按鈕（1 min / 5 min / 10 min）
- [x] 2.3 移除 `Market Hours Only` checkbox
- [x] 2.4 移除 `Save/Cancel` 按鈕
- [x] 2.5 點擊 interval 按鈕即執行儲存流程
- [x] 2.6 顯示儲存結果訊息（`Saved successfully` / `Save failed`）
- [x] 2.7 所有可點擊元件位置維持 `y < 200` 避免觸控攔截

## 3. 排程策略

- [x] 3.1 `do_fetch_quotes()` 改為非開市時間固定跳過（不依賴 `market_only` UI）

## 4. 建置整合

- [x] 4.1 `components/ui/CMakeLists.txt` 加入 `screens/screen_settings.c`
- [x] 4.2 `./flash.sh --build-only` 編譯通過

## 5. 驗證

- [x] 5.1 透過硬體按鍵可切換到 Settings 頁面
- [x] 5.2 點擊 `1 min / 5 min / 10 min` 會即時顯示儲存成功訊息
- [x] 5.3 點擊 `5 min` 後離開再進入，仍維持 `5 min` 被選中
- [x] 5.4 非開市時間確認不抓價（monitor log 可見跳過訊息）
