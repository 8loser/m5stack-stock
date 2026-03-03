# lcd-brightness Specification

## Purpose
Provide LCD brightness control via board HAL, persistent storage, and boot-time restore.

## Requirements
### Requirement: Board HAL 亮度控制介面
`board_set_lcd_brightness(uint8_t level)` SHALL 接受 0–100 的整數百分比，透過 AXP192 DCDC3 電壓調整 LCD 背光亮度。level=0 為最暗（2.5V），level=100 為最亮（3.3V）。超出範圍的值 SHALL 被 clamp 至 [0, 100]。

#### Scenario: 設定有效亮度值
- **WHEN** 呼叫 `board_set_lcd_brightness(50)`
- **THEN** AXP192 DCDC3 電壓設為約 2.9V，函式回傳 `ESP_OK`

#### Scenario: 值超出上限
- **WHEN** 呼叫 `board_set_lcd_brightness(200)`
- **THEN** 系統視為 100，設為最大亮度，回傳 `ESP_OK`

### Requirement: NVS 亮度持久化
`storage_display_save(uint8_t brightness)` SHALL 將亮度值（0–100）寫入 NVS namespace `display`，key `brightness`（u8）。`storage_display_load(uint8_t *out)` SHALL 讀取並回傳儲存值；若 key 不存在，SHALL 將 `*out` 設為預設值 80 並回傳 `ESP_OK`。

#### Scenario: 首次讀取（key 不存在）
- **WHEN** NVS 無 `display/brightness` 記錄時呼叫 `storage_display_load(&val)`
- **THEN** `val` 被設為 80，函式回傳 `ESP_OK`

#### Scenario: 儲存並讀回
- **WHEN** 呼叫 `storage_display_save(60)` 成功後呼叫 `storage_display_load(&val)`
- **THEN** `val` 為 60

### Requirement: 開機套用亮度
系統開機初始化時 SHALL 從 NVS 讀取亮度值並呼叫 `board_set_lcd_brightness()`，使螢幕呈現上次設定的亮度。套用時機 SHALL 在 `board_init()` 完成後、UI 顯示前。

#### Scenario: 正常開機
- **WHEN** 系統開機，NVS 有儲存亮度值 60
- **THEN** 背光在 UI 顯示前設為 60%

#### Scenario: NVS 無記錄開機
- **WHEN** 系統開機，NVS 無亮度記錄（首次開機或清除後）
- **THEN** 背光設為預設值 80%
