## 1. 配色常數更新

- [x] 1.1 將 `COLOR_UP` 改為 `lv_color_hex(0xFF3B30)`
- [x] 1.2 將 `COLOR_DOWN` 改為 `lv_color_hex(0x30D158)`
- [x] 1.3 將 `COLOR_CARD` 改為 `lv_color_hex(0x1C2A4A)`
- [x] 1.4 將 `COLOR_FLAT` 改為 `lv_color_hex(0x555555)`（由文字色改為 border 用灰）

## 2. 卡片 Accent Border 初始化

- [x] 2.1 在 `screen_dashboard_create()` 的卡片迴圈中，對每張 `s_cards[i]` 呼叫 `lv_obj_set_style_border_side(s_cards[i], LV_BORDER_SIDE_LEFT, 0)`
- [x] 2.2 同迴圈中設定 `lv_obj_set_style_border_width(s_cards[i], 4, 0)`

## 3. apply_card_widgets 漲跌邏輯改寫

- [x] 3.1 移除原本依 `change_percent` 設定背景 tint 的 `lv_obj_set_style_bg_color` 呼叫
- [x] 3.2 新增 accent 顏色計算：`lv_color_t accent = (q->change_percent > 0.01f) ? COLOR_UP : (q->change_percent < -0.01f) ? COLOR_DOWN : COLOR_FLAT`
- [x] 3.3 以 `lv_obj_set_style_border_color(s_cards[idx], accent, 0)` 設定左側邊框色
- [x] 3.4 以 `lv_obj_set_style_bg_color(s_cards[idx], COLOR_CARD, 0)` 統一設定背景色

## 4. 名稱標籤位移

- [x] 4.1 在 `ensure_card_widgets()` 中將 `lv_obj_set_pos(s_name_labels[idx], 4, 4)` 改為 `lv_obj_set_pos(s_name_labels[idx], 10, 2)`

## 5. Footer 單行左右分置

- [x] 5.1 在 `screen_dashboard_create()` 中將 `s_update_label` 的 `lv_obj_set_pos` 改為 `(4, 220)`，初始文字改為 `"Upd --:--:--"`
- [x] 5.2 將 `s_next_label` 的 `lv_obj_set_pos(4, 220)` 改為 `lv_obj_align(s_next_label, LV_ALIGN_BOTTOM_RIGHT, -4, -4)`，移除原本的 `lv_obj_set_pos`
- [x] 5.3 在 `apply_card_widgets()` 中將 `snprintf(time_str, ..., "更新: %s", ...)` 改為 `snprintf(time_str, ..., "Upd %s", ...)`

## 6. 驗證

- [x] 6.1 `./flash.sh --build-only` 編譯無錯誤
- [x] 6.2 燒錄後目視確認：漲股紅色左邊框、跌股綠色左邊框、平盤灰色左邊框
- [ ] 6.3 目視確認：所有卡片背景統一深藍，無 tint
- [x] 6.4 目視確認：Footer 同行，左側 `Upd`、右側 `Next`，不重疊
