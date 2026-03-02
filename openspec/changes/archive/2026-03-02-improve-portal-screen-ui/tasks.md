## 1. screen_portal.c：移除狀態標籤

- [x] 1.1 刪除 `s_status_lbl` 靜態變數宣告（第 15 行）
- [x] 1.2 刪除 `screen_portal_create()` 中建立 `s_status_lbl` 的程式碼區塊（約第 97–103 行）
- [x] 1.3 移除 `update_portal_ui()` 中所有對 `s_status_lbl` 的操作（`lv_label_set_text`、`lv_obj_set_style_text_color`）

## 2. screen_portal.c：移除 left_panel，重掛 QR code 與提示標籤

- [x] 2.1 刪除 `left_panel` 建立與樣式設定程式碼（`lv_obj_create`、`lv_obj_set_size`、`lv_obj_set_pos`、`lv_obj_set_style_*`）
- [x] 2.2 將 `s_qr_hint_lbl` 的 parent 改為 `s_qr_area`，設定明確位置與大小（x=4, y=2, w=156, h=196），保留文字對齊與字型設定
- [x] 2.3 將 `s_qr_obj`（`lv_qrcode_create`）的 parent 改為 `s_qr_area`，並將對齊方式改為 `lv_obj_set_pos(s_qr_obj, 13, 6)`

## 3. status_bar.c：簡化 Portal 頁面標題

- [x] 3.1 在 `refresh_page_message()` 的 `switch (s_page)` 中新增 `case SCREEN_PORTAL: page_name = "Portal"; break;`
- [x] 3.2 刪除 `refresh_page_message()` 中 `if (s_page == SCREEN_PORTAL)` 的特殊處理區塊（動態連線狀態字串邏輯，約第 122–133 行）
- [x] 3.3 確認 `status_bar_update_wifi()` 不再需要觸發 Portal 特殊文字（其仍可呼叫 `refresh_page_message()`，行為自然正確）

## 4. 驗證

- [x] 4.1 Build 確認無編譯錯誤（`./flash.sh --build-only`）
- [x] 4.2 燒錄後進入 Portal 頁面，確認頂部無「入口已啟動」文字標籤
- [x] 4.3 確認 QR code 後方無藍色背景方塊
- [x] 4.4 確認 status bar 固定顯示 "Portal"，不隨 WiFi 狀態變化
- [x] 4.5 確認未啟動狀態下 `s_qr_hint_lbl` 位置正確，不超出左側區域
