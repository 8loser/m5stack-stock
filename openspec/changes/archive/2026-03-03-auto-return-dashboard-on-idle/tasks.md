## 1. Idle Timeout Core Logic (ui_manager)

- [x] 1.1 在 `ui_manager.c` 新增閒置追蹤狀態（最後觸控時間、最後自動返回時間）。
- [x] 1.2 在 `lvgl_touch_cb()` 中，對所有 `pt.pressed` 事件更新最後觸控時間（包含底部虛擬按鍵區）。
- [x] 1.3 在 `lvgl_task()` 中加入 timeout 檢查：僅非 Dashboard/Portal 且亮屏時計算，達 10 秒時切回 Dashboard。
- [x] 1.4 在 `ui_manager_switch_screen()` 切到非 Dashboard/Portal 時重置閒置基準，避免立即觸發舊超時。
- [x] 1.5 在 `lvgl_task()` 偵測熄屏→亮屏轉換時，自動切回 Dashboard。

## 2. Configuration

- [x] 2.1 在 `include/app_config.h` 新增 `UI_NON_HOME_IDLE_RETURN_MS` 常數（10000ms）。
- [x] 2.2 移除 `ui_manager.c` 內 magic number，統一使用設定常數。

## 3. Logging and Guard

- [x] 3.1 新增自動返回 log（例如 `idle timeout, return to dashboard`）。
- [x] 3.2 加入重複觸發保護（短時間內不重入 auto-return）。

## 4. Validation

- [x] 4.1 `./flash.sh --build-only` 必須通過。
- [x] 4.2 手動測試：Log/Info/Settings/HW Test 各頁閒置 10 秒自動回 Dashboard。
- [x] 4.3 手動測試：Dashboard 與 Portal 不受影響。
- [x] 4.4 手動測試：熄屏期間暫停計時，亮屏後恢復。（N/A：亮屏固定顯示 Dashboard，此路徑由 4.6 覆蓋）
- [x] 4.5 手動測試：任意觸控按下（含底部虛擬按鍵）可重置倒數。
- [x] 4.6 手動測試：在非 Dashboard 頁面熄屏再亮屏，應立即顯示 Dashboard。
