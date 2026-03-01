## 1. ui_manager.c — 資料結構與靜態清單

- [x] 1.1 在 `ui_manager.c` 頂部加入 `s_nav_screens[]` 靜態陣列（含 SCREEN_DASHBOARD、SCREEN_LOG、SCREEN_INFO）
- [x] 1.2 加入 `static int s_nav_idx = 0;` 追蹤當前輪詢位置
- [x] 1.3 不使用 `s_prev_screen`，Portal 返回目標固定為 `SCREEN_DASHBOARD`

## 2. ui_manager.c — 重寫 handle_hw_button()

- [x] 2.1 移除現有的 `switch (s_cur_screen)` per-screen 分支
- [x] 2.2 實作 Portal 分支：btn0/1/2 → `switch_screen(SCREEN_DASHBOARD)`
- [x] 2.3 實作非 Portal 分支：btn1 → 切換到 SCREEN_PORTAL
- [x] 2.4 實作 btn0（左）：`s_nav_idx` 遞減（含 wraparound），切換到 `s_nav_screens[s_nav_idx]`
- [x] 2.5 實作 btn2（右）：`s_nav_idx` 遞增（含 wraparound），切換到 `s_nav_screens[s_nav_idx]`

## 3. screen_dashboard.c — 解耦刷新邏輯

- [x] 3.1 確認 `screen_dashboard_on_btn()` 是否仍被任何地方呼叫
- [x] 3.2 移除 `screen_dashboard_on_btn()` 函數（或其中的按鍵刷新邏輯）
- [x] 3.3 確認 Dashboard 觸控 UI 上已有刷新按鈕可觸發報價更新（若無則新增）

## 4. 驗證

- [ ] 4.1 在 Dashboard 頁按 btn1 → 進入 Portal；再按 btn1 → 回到 Dashboard
- [ ] 4.2 在 Dashboard 頁按 btn2 → 切換到下一頁（Log 或 Info）
- [ ] 4.3 在任意非 Portal 頁按 btn1 → 進入 Portal；在 Portal 按 btn1 → 回到 Dashboard
- [ ] 4.4 在 Portal 頁按 btn0 或 btn2 → 回到 Dashboard
- [ ] 4.5 在 Dashboard 頁按 btn0 → 輪詢到最後一頁（wraparound）
