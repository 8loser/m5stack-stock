# settings-page Specification

## Purpose
TBD - created by archiving change add-settings-screen. Update Purpose after archive.
## Requirements
### Requirement: 輪詢間隔三鍵切換
Settings 頁面 SHALL 提供 3 個 interval 按鈕：`1 min` / `5 min` / `10 min`，對應秒數 [60, 300, 600]。進入頁面時 SHALL 根據 `scheduler_get_config().quote_interval_s` 預選最接近的按鈕。

#### Scenario: 載入當前設定
- **WHEN** 使用者進入 Settings 頁面（`screen_settings_load()` 被呼叫）
- **THEN** 三個 interval 按鈕其中一個呈現選中狀態，對應當前 `quote_interval_s`（最接近匹配）

#### Scenario: 當前值不在選項中
- **WHEN** NVS 儲存的 `quote_interval_s` 不在 [60,300,600] 中
- **THEN** 按鈕選中最接近的選項（fallback 為 `1 min`）

### Requirement: 點擊按鈕即時儲存
使用者點擊任一 interval 按鈕時，Settings 頁面 SHALL 立即：
1. 更新 `cfg.quote_interval_s`
2. 呼叫 `scheduler_apply_config(&cfg)` 使設定即時生效
3. 呼叫 `storage_schedule_save(&cfg)` 持久化到 NVS

#### Scenario: 點擊 5 min
- **WHEN** 使用者點擊 `5 min`
- **THEN** `quote_interval_s` 變為 300，並立即套用與儲存

### Requirement: 顯示儲存結果訊息
Settings 頁面 SHALL 在每次儲存後顯示結果文字訊息：
- 成功顯示 `Saved successfully`
- 失敗顯示 `Save failed`

#### Scenario: 儲存成功
- **WHEN** `scheduler_apply_config()` 與 `storage_schedule_save()` 都成功
- **THEN** 畫面顯示 `Saved successfully`

#### Scenario: 儲存失敗
- **WHEN** 任一儲存流程回傳錯誤
- **THEN** 畫面顯示 `Save failed`

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
