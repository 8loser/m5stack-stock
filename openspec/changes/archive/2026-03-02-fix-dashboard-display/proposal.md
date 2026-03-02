## Why

Dashboard 有三個顯示問題：股票代碼文字被截斷（LVGL padding 造成 clip）、底部缺少下次更新時間、row 數固定為 5 而非跟隨實際設定的股票數量。

## What Changes

- 移除 card 預設 padding，修復股票代碼上半部被截斷的問題
- Dashboard 底部新增 "Next: HH:MM:SS" 標籤，顯示下次報價更新的預計時間
- Dashboard 改為固定 5 列顯示，當股票數超過 5 時每頁 5 檔輪換內容（不做捲動動畫）
- `scheduler` 新增 API 供外部查詢距下次報價觸發的剩餘秒數

## Capabilities

### New Capabilities

- `dashboard-display`: Dashboard 頁面的 layout 修正與固定 5 列輪頁顯示，包含 padding fix、next-update 標籤、分頁內容輪換控制

### Modified Capabilities

（無現有 spec 需異動）

## Impact

| 檔案 | 異動說明 |
|------|---------|
| `components/ui/screens/screen_dashboard.c` | padding fix、新增 s_next_label、card 全部預設 hidden、新增 set_card_count |
| `components/scheduler/scheduler.h` | 新增 `scheduler_get_seconds_to_next_quote()` |
| `components/scheduler/scheduler.c` | 實作上述 API，使用 `xTimerGetExpiryTime` |
| `components/ui/ui_manager.h` | 新增 `ui_manager_set_dashboard_card_count()` |
| `components/ui/ui_manager.c` | 實作上述包裝函式 |
| `main/main.c` | storage_init 後讀 stock list，呼叫 set_card_count；quote 更新後同步 next time |
