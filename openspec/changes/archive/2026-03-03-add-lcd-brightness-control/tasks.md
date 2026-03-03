## 1. Storage 層

- [x] 1.1 在 `components/storage/include/storage.h` 新增 `storage_display_save(uint8_t brightness)` 與 `storage_display_load(uint8_t *out)` 宣告
- [x] 1.2 在 `components/storage/storage.c` 實作兩函式，NVS namespace `display`，key `brightness`（u8），load 不存在時回傳預設值 80

## 2. Board HAL 層

- [x] 2.1 在 `components/board/include/board.h` 新增 `board_set_lcd_brightness(uint8_t level)` 宣告（0–100）
- [x] 2.2 在 `components/board/board.c` 實作，clamp level 至 [0,100]，換算為 0–255 後呼叫 `axp192_set_lcd_backlight()`

## 3. 開機套用

- [x] 3.1 在 `main/main.c` 的 `board_init()` 完成後、`ui_manager_init()` 之前，呼叫 `storage_display_load()` 與 `board_set_lcd_brightness()` 套用開機亮度

## 4. Settings UI

- [x] 4.1 在 `components/ui/screens/screen_settings.c` 新增亮度 slider（`lv_slider`，range 0–100）與標籤（`Brightness: XX%`）
- [x] 4.2 在 `screen_settings_load()` 中呼叫 `storage_display_load()` 初始化 slider 位置與標籤文字
- [x] 4.3 `LV_EVENT_VALUE_CHANGED` callback：呼叫 `board_set_lcd_brightness()` 即時調整，更新標籤
- [x] 4.4 `LV_EVENT_RELEASED` callback：呼叫 `storage_display_save()` 持久化當前值
