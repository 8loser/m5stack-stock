## Context

AXP192 是 M5Stack Core2 的電源管理 IC，透過 I2C 提供電池電壓 ADC 讀值。REG 0x78（高位元組）與 REG 0x79（低位元組）共同編碼一個 12-bit ADC 值，解析度 1.1mV/bit。

現有程式碼 `axp192_get_battery_voltage()` 的合併公式：
```c
uint16_t raw = ((uint16_t)(h & 0x7F) << 5) | (l & 0x1F);
```
有兩個錯誤：
1. `h & 0x7F`：遮蔽高位元組的 bit 7，但電池正常電壓範圍（3.0V–4.2V）對應的 ADC 高位元組值（170–238）**全部** bit 7 都是 1，導致每次讀值都偏低
2. `<< 5`：應為 `<< 4`（REG 0x78 存的是 ADC[11:4]，接 REG 0x79 的 ADC[3:0]）

上次嘗試修正（commit d283c3d）誤判為充電電流不足，將充電設定從 0xC0 改為 0xC8（300mA → 780mA），但電量顯示問題與充電電流無關。

## Goals / Non-Goals

**Goals:**
- 電量讀值與 AXP192 datasheet 一致，滿電 4.2V 顯示 100%
- 充電中仍可正確反映當前電量，並以 `~` 前綴標示

**Non-Goals:**
- 改用 Coulomb counter（AXP192 有此功能，但需要額外啟用與校正，超出此次修正範疇）
- 改善電量估算曲線（線性 voltage-to-percent 已足夠此應用）

## Decisions

**決策：只修正位元解析公式，不引入 Coulomb counter**

AXP192 的 Coulomb counter 可提供更精確的電量估算（考慮溫度、充放電效率），但需要：
- 啟用 ADC 控制暫存器中的 Coulomb counter 位元
- 清零計數器後校正滿電狀態
- 額外的暫存器讀寫（REG 0xB0~0xB3）

現有電壓映射方案（3.0V=0%，4.2V=100%，線性）在常溫下足夠準確，且問題根本是公式寫錯。修正一行即可完全解決，無需增加複雜度。

**修正公式**（符合 AXP192 datasheet p.24）：
```c
// REG 0x78 = ADC[11:4]，REG 0x79 低 4 bits = ADC[3:0]
uint16_t raw = ((uint16_t)h << 4) | (l & 0x0F);
return (float)raw * 1.1f / 1000.0f;
```

## Risks / Trade-offs

- **[風險] 充電電壓干擾**：電池接受充電時，端電壓略高於靜置電壓（可能顯示略超出實際 SOC）
  → 緩解：現有 `~` 前綴已標示充電中，屬預期行為；線性估算在此場景屬已知限制

- **[風險] 公式修正後數值跳變**：上電後首次顯示可能從原本的「73%」直接跳到正確的「100%」
  → 緩解：這正是期望的修正行為，非 regression
