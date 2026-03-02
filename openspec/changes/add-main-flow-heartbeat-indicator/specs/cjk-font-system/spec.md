## ADDED Requirements

### Requirement: 心跳 emoji 字元納入 UI 字型字符集
字型產生流程 SHALL 將心跳指標使用的 emoji 字元納入 `ui_symbols.txt` 來源集合，並隨 `generate_fonts.sh` 輸出到 `lv_font_noto_tc_14.c` 與 `lv_font_noto_tc_16.c`。

#### Scenario: 重新產生字型後含心跳 emoji
- **WHEN** 執行 `./generate_fonts.sh`
- **THEN** 產生的 Noto TC 子集字型包含心跳 emoji 字元

### Requirement: 心跳字元渲染失敗時 fallback
當目標字型無法正確渲染心跳 emoji 時，UI SHALL fallback 使用 ASCII `<3` 作為正常心跳符號，避免顯示缺字方框。

#### Scenario: emoji 可渲染
- **WHEN** 字型包含且可渲染心跳 emoji
- **THEN** status bar 正常狀態顯示 emoji 心跳符號

#### Scenario: emoji 不可渲染
- **WHEN** 字型缺少心跳 emoji 或渲染結果為缺字
- **THEN** status bar 正常狀態改顯示 `<3`
