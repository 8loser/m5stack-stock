## ADDED Requirements

### Requirement: 亮度 Slider 元件
Settings 頁面 SHALL 新增一個亮度調整 slider，範圍 0–100，並附帶標籤顯示目前百分比數值（如 `Brightness: 80%`）。進入頁面時 SHALL 根據 `storage_display_load()` 讀取的值初始化 slider 位置。

#### Scenario: 進入 Settings 頁面
- **WHEN** 使用者進入 Settings 頁面（`screen_settings_load()` 被呼叫）
- **THEN** 亮度 slider 位置反映 NVS 儲存的亮度值，標籤顯示對應百分比

#### Scenario: NVS 無記錄時進入頁面
- **WHEN** NVS 無亮度記錄時進入 Settings 頁面
- **THEN** slider 預設位置為 80，標籤顯示 `Brightness: 80%`

### Requirement: 拖曳即時調整背光
使用者拖曳亮度 slider 時，Settings 頁面 SHALL 即時呼叫 `board_set_lcd_brightness()` 反映畫面亮度變化，無需等待放開。

#### Scenario: 拖曳 slider
- **WHEN** 使用者拖曳亮度 slider 至任意值
- **THEN** 螢幕背光立即對應調整，標籤即時更新顯示新百分比

### Requirement: 放開時儲存亮度
使用者放開亮度 slider 時，Settings 頁面 SHALL 呼叫 `storage_display_save()` 將當前值持久化到 NVS。

#### Scenario: 放開 slider
- **WHEN** 使用者拖曳 slider 後放開，值為 45
- **THEN** `storage_display_save(45)` 被呼叫，亮度值 45 寫入 NVS
