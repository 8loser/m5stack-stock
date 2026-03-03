## Context

M5Stack Core2 使用 AXP192 電源管理 IC，LCD 背光電源由 DCDC3 輸出電壓控制（2.5V–3.3V）。`axp192_set_lcd_backlight(uint8_t brightness)` 已在 `components/board/axp192.c` 實作，映射 0–255 → 2.5V–3.3V，但此函式未透過 board HAL 層暴露，UI 也無對應設定介面。

## Goals / Non-Goals

**Goals:**
- 在 board HAL 暴露 0–100 百分比亮度控制介面
- 開機從 NVS 讀取上次亮度並套用
- Settings 頁面新增 slider，即時調整且持久化

**Non-Goals:**
- 自動亮度（環境光感測）
- 漸變動畫
- 每個頁面獨立亮度

## Decisions

### 1. Board HAL API 使用 0–100 百分比

`board_set_lcd_brightness(uint8_t level)` 接受 0–100，內部轉換為 0–255 再呼叫 `axp192_set_lcd_backlight()`。

**理由**: UI slider 與 NVS 統一使用整數百分比，避免 0–255 scale 洩漏到上層。level 0 不切斷 DCDC3 電源（仍留最低電壓 2.5V），避免顯示器完全熄滅後無法喚醒。

**替代方案**: 直接用 0–255。捨棄原因：語意不直觀，UI slider 需額外換算。

### 2. NVS namespace `display`，key `brightness`（u8）

沿用專案現有 NVS 命名規則（per-feature namespace）。型別 u8 足夠（0–100）。

**預設值**: 80（對應約 2.96V，亮度適中）。

### 3. storage 層新增 `storage_display_save/load`

與現有 `storage_schedule_save/load` 模式一致，封裝 NVS 讀寫細節，不在 UI 或 main 直接操作 NVS。

### 4. Settings UI 使用 `lv_slider`

LVGL 內建 `lv_slider`，range 0–100，`LV_EVENT_VALUE_CHANGED` 觸發即時套用，`LV_EVENT_RELEASED` 時儲存 NVS（避免拖曳過程頻繁寫 NVS）。

**替代方案**: 分段按鈕（25/50/75/100）。捨棄原因：連續 slider 體驗更佳。

### 5. 開機套用時機

在 `board_init()` 完成後、`ui_manager_init()` 之前，於 `main.c` 讀取 NVS 並呼叫 `board_set_lcd_brightness()`。NVS 讀取失敗則使用預設值 80。

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| level=0 時畫面仍有微弱背光（2.5V）| 設計決策，level=0 視為「最暗」非「關閉」，文件說明 |
| NVS 首次讀取 `ESP_ERR_NVS_NOT_FOUND` | storage_display_load 回傳預設值 80，非錯誤 |
| slider 拖曳時 UI thread 頻繁呼叫 board HAL | VALUE_CHANGED 即時調整（純 I2C 寫入，<1ms），RELEASED 才寫 NVS |
| AXP192 I2C 呼叫在 LVGL callback 中執行 | AXP192 I2C 已有 mutex 保護；與 LVGL mutex 無交叉持有，無死鎖風險 |
