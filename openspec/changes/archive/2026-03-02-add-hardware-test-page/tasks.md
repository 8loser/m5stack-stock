## 1. 導航與頁面註冊（Core2）

- [x] 1.1 在 `screen_id_t` 新增 `SCREEN_HW_TEST`
- [x] 1.2 在 `ui_manager_init()` 註冊 `screen_hw_test_create()` 到 `s_screens[SCREEN_HW_TEST]`
- [x] 1.3 將 `SCREEN_HW_TEST` 納入左右鍵頁面輪詢序列（btn=0/2）
- [x] 1.4 在 Dashboard 新增 `HW Test` 入口按鈕（不掛在 Portal）
- [x] 1.5 `components/ui/CMakeLists.txt` 加入 `screens/screen_hw_test.c`

## 2. HW Test UI 收斂（只保留必要硬體測試）

- [x] 2.1 建立 `components/ui/screens/screen_hw_test.c`
- [x] 2.2 移除頁面標題（不顯示 `HW TEST`）
- [x] 2.3 內容區塊垂直置中
- [x] 2.4 Vibration 區塊只保留 1 顆按鈕：`Vibrate`
- [x] 2.5 Audio 區塊只保留 1 顆按鈕：`Beep`

## 3. HW Test 動作定義

- [x] 3.1 `Vibrate` 觸發長震動（`vibration_alert()`）
- [x] 3.2 `Beep` 播放 4 個短音
- [x] 3.3 4 個短音使用不同音調（由低到高）

## 4. 硬體控制穩定化（依本次除錯）

- [x] 4.1 分離 AXP192 的 vibration 與 speaker 控制，避免互相影響
- [x] 4.2 修正 audio 測試「不會停止」問題（播放後正確關閉輸出）
- [x] 4.3 移除開機 smoke test（開機不自動震動/鳴叫）

## 5. 文件與任務一致性

- [x] 5.1 更新 `specs/` delta（`screen-portal` 改為 `screen-dashboard` 入口語義）
- [x] 5.2 sync 到 `openspec/specs/` 前先確認上述語義一致

## 6. 驗證

- [x] 6.1 Dashboard 可見 `HW Test`，可正常進入頁面
- [x] 6.2 左右鍵可輪詢到 `HW Test`
- [x] 6.3 `Vibrate` 可穩定觸發長震動
- [x] 6.4 `Beep` 每次按下會播放 4 個短音且能自動停止
- [x] 6.5 重新開機後不會卡在 HW Test，畫面可正常進入 Dashboard
