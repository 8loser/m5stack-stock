## Context

`screen_dashboard.c` 是 UI 元件中唯一需要改動的檔案。目前卡片漲跌以背景 tint 區分，但在深藍底色（`#1A1A2E`）上 `#2D0000`/`#002D00` 幾乎不可見。Footer 兩行佔用 33px 垂直空間，且中英混排。

## Goals / Non-Goals

**Goals:**
- 以左側 border 取代 bg tint，提升漲跌視覺對比
- 更新配色常數，使顏色在深色背景上更鮮明
- 名稱標籤 x 偏移讓出邊框空間
- Footer 壓縮為單行，左右分置

**Non-Goals:**
- 不改任何業務邏輯、資料流或其他 UI 頁面
- 不新增 LVGL widget 或改變 widget 數量
- 不修改字型檔案或字型設定

## Decisions

### D1：左側 Border 取代背景 Tint

LVGL 8.4 支援 `lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_LEFT, 0)`，搭配 `lv_obj_set_style_border_width` 可實現單側邊框。在 `screen_dashboard_create()` 初始化每張卡片時設定 border side/width，在 `apply_card_widgets()` 動態更新 `border_color`，同時將 `bg_color` 統一設為 `COLOR_CARD`。

**替代方案**：保留 tint 但加深顏色 → 不選，因為小螢幕上顏色感知受環境光影響大，線條比色塊更可靠。

### D2：Footer 同行左右分置

`s_update_label` 以 `lv_obj_set_pos(4, 220)` 靠左，`s_next_label` 以 `lv_obj_align(LV_ALIGN_BOTTOM_RIGHT, -4, -4)` 靠右，兩者 y 相同。文字由「更新: HH:MM:SS」縮短為「Upd HH:MM:SS」。

**替代方案**：保留兩行但縮小字型 → 不選，Montserrat 10 已是最小可讀尺寸。

### D3：配色數值

| 常數 | 舊值 | 新值 | 理由 |
|------|------|------|------|
| `COLOR_UP` | `#FF4444` | `#FF3B30` | iOS red，飽和度更高 |
| `COLOR_DOWN` | `#44BB44` | `#30D158` | iOS green，在深色背景更清晰 |
| `COLOR_CARD` | `#16213E` | `#1C2A4A` | 輕微提亮，增加卡片存在感 |
| `COLOR_FLAT` | `#FFFFFF`（文字色）| `#555555`（border 用）| 平盤邊框不搶眼 |

## Risks / Trade-offs

- `LV_BORDER_SIDE_LEFT` 需 LVGL ≥ 8.x → 專案使用 ESP-IDF v5.1.4 內建 LVGL 8.3+，已確認支援
- Footer 靠右對齊使用 `LV_ALIGN_BOTTOM_RIGHT`，如 `s_screen` 尺寸非預期可能偏移 → 低風險，screen 固定 `LCD_WIDTH x LCD_HEIGHT`

## Migration Plan

1. 直接修改 `screen_dashboard.c`，`./flash.sh --build-only` 驗證編譯
2. `./flash.sh` 目視確認邊框顏色與 footer 版面

## Open Questions

無。
