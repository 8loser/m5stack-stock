## Context

Portal 頁面（`screen_portal.c`）目前有三個視覺問題：

1. `s_status_lbl`（y=22）重複表達 portal 啟用狀態，資訊已由頂部 status bar 涵蓋
2. `left_panel`（164×196，`0x0F3460` 40% 不透明藍底）疊在 QR code（LVGL canvas，自帶白底）下方，形成「藍框內白方塊」的突兀外觀
3. Status bar 在 `SCREEN_PORTAL` 時顯示動態連線狀態字串，格式長且不一致

受影響檔案：
- `components/ui/screens/screen_portal.c`
- `components/ui/widgets/status_bar.c`

## Goals / Non-Goals

**Goals:**
- 移除 portal 頁面頂部冗余的狀態文字標籤
- 消除 QR code 後方醜陋的藍色背景方塊
- Status bar 在 portal 頁面統一顯示靜態 "Portal" 字串

**Non-Goals:**
- 不改變 portal 頁面的整體佈局結構（左側 QR / 右側資訊分欄）
- 不改變 QR code 的內容或 WIFI URI 邏輯
- 不修改 portal 的啟動/停止行為

## Decisions

**D1：移除 `s_status_lbl` 而非隱藏**

選擇直接移除而非用 `LV_OBJ_FLAG_HIDDEN` 隱藏，因為此標籤已無存在意義，保留只會增加維護成本。`update_portal_ui()` 中對 `s_status_lbl` 的所有操作一併移除。

**D2：直接移除 `left_panel`**

`left_panel` 除了醜陋的藍色背景外無其他必要用途。`lv_qrcode` canvas 自帶白底，視覺不依賴 parent 背景。選擇移除 `left_panel`，將 `s_qr_obj` 和 `s_qr_hint_lbl` 改 parent 為 `s_qr_area`，並以明確座標定位：

- `s_qr_obj`：`lv_obj_set_pos(s_qr_obj, 13, 6)`（原 left_panel 在 x=4，內部 TOP_MID 換算）
- `s_qr_hint_lbl`：`lv_obj_set_pos` + `lv_obj_set_size` 明確指定位置，置中由 `LV_TEXT_ALIGN_CENTER` 保留

**D3：Status bar 使用靜態 "Portal" 字串**

`refresh_page_message()` 中 `SCREEN_PORTAL` 的特殊處理（含 wifi 狀態判斷）全部刪除，改為 `page_name = "Portal"` 並走 default `snprintf` 路徑。WiFi 狀態已由頂部 WiFi icon 的顏色（綠/黃/紅）表達，status bar 不需重複。

## Risks / Trade-offs

- [風險] `s_qr_area` y=38 是原先 `s_status_lbl`（高約 18px）下方的位置；移除 label 後頂部 y=22-38 會有空白 → 可選擇將 `s_qr_area` 上移至 y=22 填補空間，或保持現狀（小空白無害）。保持現狀以最小化改動。

## Open Questions

（無）
