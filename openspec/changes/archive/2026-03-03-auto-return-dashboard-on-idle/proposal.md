## Why

目前在 Log/Info/Settings/HW Test 等頁面停留時，若使用者離開裝置，畫面會長時間停在非首頁。需求希望在非 Portal、非 Dashboard 畫面偵測觸控閒置，超過 10 秒自動返回 Dashboard，讓裝置回到主要監看頁。

## What Changes

- 在 `ui_manager` 新增「非首頁畫面閒置返回」行為：
  - 只在 `SCREEN_LOG / SCREEN_INFO / SCREEN_SETTINGS / SCREEN_HW_TEST` 生效
  - 閒置門檻 10 秒（可由常數設定）
  - 無觸控按下事件時觸發回到 `SCREEN_DASHBOARD`
- 閒置計時在螢幕熄滅（`board_is_screen_on()==false`）時暫停
- 任意觸控按下（含底部虛擬按鍵區）都會重置倒數

## Capabilities

### Modified Capabilities

- `ui-manager`: 新增非首頁閒置超時自動返回 Dashboard 行為
- `ui-manager`: 觸控事件將更新閒置時間基準，用於超時計算

## Impact

- `components/ui/ui_manager.c`：新增閒置時間狀態、觸控重置邏輯、LVGL loop 超時檢查
- `include/app_config.h`：新增 UI 閒置返回 timeout 常數
- 無 NVS/網路/排程資料格式變更
