## Why

開發和除錯時需要快速驗證 M5Core2 硬體元件（震動馬達、蜂鳴器/喇叭）是否正常運作，目前缺乏即時的互動式硬體測試介面，需要靠燒錄獨立測試碼才能確認。

## What Changes

- 新增 `SCREEN_HW_TEST` 頁面（第三個頁面，id = 2）
- 頁面包含多個測試按鈕，每個按鈕觸發對應的硬體動作：
  - **Vibration** 區塊：Haptic（短震）、Alert（長震）、Pulse 500ms
  - **Audio** 區塊：Beep 1kHz、Alert Up（漲停音）、Alert Down（跌停音）、Notify
- 從 Settings / Portal 頁面可導航進入此頁（或透過硬體按鍵）
- 更新 `screen_id_t` enum、`ui_manager` 頁面陣列與按鍵路由

## Capabilities

### New Capabilities
- `hw-test-page`: 硬體測試頁面，提供震動與音效的互動式測試按鈕

### Modified Capabilities
- `ui-manager`: 新增 `SCREEN_HW_TEST` enum 值與對應頁面初始化、切換邏輯

## Impact

- `components/ui/include/ui_manager.h` — 新增 `SCREEN_HW_TEST` enum
- `components/ui/ui_manager.c` — 初始化第三頁、按鍵路由更新
- `components/ui/screens/screen_hw_test.c`（新增）— 頁面實作
- `components/ui/CMakeLists.txt` — 加入新 source 檔
- 依賴現有 HAL API：`vibration_haptic/alert/pulse`、`audio_beep/alert_up/alert_down/notify`
