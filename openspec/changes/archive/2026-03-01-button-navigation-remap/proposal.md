## Why

目前底部三個虛擬按鍵的行為零散且依頁面而異（Dashboard: btn1=刷新/btn2=Portal；Portal: btn0=返回），缺乏一致的導航心智模型。隨著未來頁面增加，現有的 per-screen switch-case 架構將難以維護。需要建立統一的按鍵導航語義：中間 = 進入 Portal，左右 = 在主頁面間輪詢。

## What Changes

- `btn 1`（中）：從任意非 Portal 頁面切換到 Portal；在 Portal 時回到 Dashboard
- `btn 0`（左）：在「可輪詢頁面」清單中向左切換；在 Portal 時回到 Dashboard
- `btn 2`（右）：在「可輪詢頁面」清單中向右切換；在 Portal 時回到 Dashboard
- Portal 頁面不加入可輪詢清單，維持「快捷入口」語義
- `handle_hw_button` 重構：移除 per-screen switch-case，改用統一的全域導航邏輯
- `screen_dashboard_on_btn(1)`（刷新報價）的功能改由 Dashboard 頁面自行處理（觸控 UI 按鈕），不再依賴實體按鍵

## Capabilities

### New Capabilities

- `hw-button-navigation`: 統一的底部虛擬按鍵導航語義——中間進 Portal、左右輪詢主頁面清單，供 `handle_hw_button` 實作

### Modified Capabilities

（無現有 spec，首次建立）

## Impact

- `components/ui/ui_manager.c`：重構 `handle_hw_button()`，新增可輪詢頁面清單（`s_nav_screens[]`）與當前索引追蹤
- `components/ui/include/ui_manager.h`：`screen_id_t` 可能擴充（預留未來頁面）
- `components/ui/screens/screen_dashboard.c`：`screen_dashboard_on_btn()` 可評估是否保留或移除
- 不影響 `ui_manager_switch_screen()` 的對外 API，Portal 開啟/關閉側效應維持不變
