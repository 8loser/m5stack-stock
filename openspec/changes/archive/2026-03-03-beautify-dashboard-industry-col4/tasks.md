## 1. 調整欄寬分配

- [x] 1.1 修改 `compute_card_layout()` 中的欄寬變數，改為 `w_name=85, w_price=68, w_change=55, w_industry=remainder`
- [x] 1.2 確認 `s_col_w[0..3]` 與 `s_col_x[0..3]` 的賦值順序對應新欄位語義（Name / Price / Change% / Industry）

## 2. 重新對應 widget 位置

- [x] 2.1 在 `ensure_card_widgets()` 中將 `s_price_labels[idx]` 的 `lv_obj_set_pos` 改為 `s_col_x[1]`，size 改為 `s_col_w[1]`
- [x] 2.2 在 `ensure_card_widgets()` 中將 `s_change_labels[idx]` 的 `lv_obj_set_pos` 改為 `s_col_x[2]`，size 改為 `s_col_w[2]`
- [x] 2.3 在 `ensure_card_widgets()` 中將 `s_industry_labels[idx]` 的 `lv_obj_set_pos` 改為 `s_col_x[3]`，size 改為 `s_col_w[3]`

## 3. 驗證

- [x] 3.1 `./flash.sh --build-only` 確認 build 無 warning/error
- [x] 3.2 燒錄後確認 dashboard 欄位順序為 Name | Price | Change% | Industry
- [x] 3.3 確認 Name 欄能完整顯示 symbol+name 兩行
- [ ] 3.4 確認 Industry 欄超長時顯示 `...` 截斷，不影響 Price / Change% 欄
- [x] 3.5 確認漲跌色與左側邊框顏色仍正常運作
