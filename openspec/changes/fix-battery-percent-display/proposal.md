## Why

AXP192 電池電壓 ADC 暫存器（0x78/0x79）的位元解析公式錯誤，導致電量百分比系統性偏低：滿電 4.2V 被算成約 3.88V，顯示只有 73%。裝置插著充電也永遠停在「70幾%」，無法反映真實電量。

## What Changes

- 修正 `axp192_get_battery_voltage()` 的 ADC 原始值合併公式
  - 舊：`((uint16_t)(h & 0x7F) << 5) | (l & 0x1F)` — 遮蔽 bit 7 且偏移量錯誤
  - 新：`((uint16_t)h << 4) | (l & 0x0F)` — 符合 AXP192 datasheet（REG 0x78 = ADC[11:4]，REG 0x79 = ADC[3:0]）

## Capabilities

### New Capabilities

- `battery-display`: 電池電量讀取與顯示的正確性規格，包含 AXP192 ADC 解析、充電狀態偵測、狀態列呈現行為。

### Modified Capabilities

（無，此修正不改變任何現有 spec 的需求）

## Impact

- `components/board/axp192.c`：`axp192_get_battery_voltage()` 函式，一行修正
- `components/board/include/axp192.h`：無需修改
- `components/ui/widgets/status_bar.c`：無需修改（邏輯正確，只是吃到錯誤的數值）
- 無 API 異動、無 breaking change
