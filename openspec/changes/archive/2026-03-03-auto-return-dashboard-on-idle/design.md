## Context

`ui_manager` 已集中管理：
- 觸控讀取回呼（`lvgl_touch_cb`）
- 畫面切換（`ui_manager_switch_screen`）
- LVGL 主迴圈（`lvgl_task`）

因此閒置返回最佳掛點是在同一模組內完成，不需新增 task 或跨模組同步。

## Goals / Non-Goals

**Goals:**
- 非 Portal、非 Dashboard 畫面，無觸控 10 秒自動返回 Dashboard
- 熄屏時暫停閒置倒數
- 任意觸控按下（含底部虛擬按鍵）重置倒數
- 維持 LVGL thread-safety（持續使用既有 mutex 模式）

**Non-Goals:**
- 不新增設定頁調整 timeout
- 不改變現有硬體按鍵導航語意
- 不變更 Portal 自動啟停邏輯

## Decisions

1. 觸控時間基準放在 `lvgl_touch_cb`
- 決策：`pt.pressed` 時更新 `s_last_touch_ms`
- 原因：所有觸控來源已集中於此，包含底部虛擬按鍵

2. 超時檢查放在 `lvgl_task`
- 決策：在既有 LVGL loop 每輪檢查是否超時
- 原因：可避免新增背景 task，降低同步複雜度

3. 只在特定 screen 生效
- 決策：`id != SCREEN_DASHBOARD && id != SCREEN_PORTAL` 才觸發
- 原因：符合需求邊界

4. 熄屏暫停計時
- 決策：`!board_is_screen_on()` 時跳過超時判定
- 原因：避免熄屏期間被動超時造成誤跳頁

## Risks / Trade-offs

- [Risk] 邊界時刻重複觸發切頁
  - Mitigation: 記錄最後自動返回時間並做最小保護間隔
- [Risk] 切入非首頁後立即被舊計時值踢回
  - Mitigation: `ui_manager_switch_screen()` 切到非首頁時重置 `s_last_touch_ms`

## Validation Plan

- 編譯檢查：`./flash.sh --build-only`
- 實機檢查：
  - 非首頁靜置 >10 秒會回 Dashboard
  - Portal/Dashboard 靜置不觸發
  - 熄屏等待 >10 秒後亮屏不立即跳頁
  - 觸控與底部虛擬按鍵可重置倒數
