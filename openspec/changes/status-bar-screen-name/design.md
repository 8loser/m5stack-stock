## Context

Status bar 由 `components/ui/widgets/status_bar.c` 實作，建立在 `lv_layer_top()` 上，包含：

- 左側：時間（`s_time_lbl`）
- 中央：`s_msg_lbl`（LV_LABEL_LONG_SCROLL_CIRCULAR，目前顯示 `"{page} - TWSE monitor ({WiFi})"`）
- 右側：WiFi 圖示（`s_wifi_lbl`）＋電量（`s_batt_lbl`）

`refresh_page_message()` 已根據 `s_page` 組出中央文字，並由 `status_bar_set_page(id)` 觸發更新。`ui_manager_switch_screen()` 在每次切換時呼叫 `status_bar_set_page(id)`。

**現有問題**：`refresh_page_message()` 的 screen name 對照表只處理 `SCREEN_LOG` 和 `SCREEN_INFO`，其餘（包含 `SCREEN_SETTINGS`）都 fallthrough 為 `"Dashboard"`，導致切到 Settings 時標題顯示錯誤。

## Goals / Non-Goals

**Goals:**
- 所有 5 個 screen（Dashboard / Log / Info / Settings / Portal）在切換後，status bar 中央區域顯示正確的 screen 名稱
- Settings screen 不再顯示 "Dashboard"

**Non-Goals:**
- 不變更 status bar 的排版或視覺設計
- 不新增獨立的 screen 名稱專用 label
- 不改變中央文字的訊息格式（保留 `"{name} - TWSE monitor (...)"` 結構）

## Decisions

### Decision 1：補全 screen name 對照表於 `refresh_page_message()`

在 `refresh_page_message()` 中補上 `SCREEN_SETTINGS`（顯示 `"Settings"`），並確保 `SCREEN_DASHBOARD` 明確對應 `"Dashboard"`（不依賴 fallthrough）。

**Alternatives considered:**
- 使用 `switch` 陳述式取代 if/else：可讀性更好、compiler 可警告未處理的 enum case → 採用此方式
- 在 `ui_manager.h` 建立 `screen_id_to_name()` getter：過度設計，目前僅 `status_bar.c` 需要此對照 → 不採用

### Decision 2：Portal screen 訊息格式維持不變

Portal 已有特殊格式（`"Portal Setup - Connected/Connecting/Offline"`），含 WiFi 狀態語意，應保留。

## Risks / Trade-offs

- `SCREEN_COUNT` 作為哨兵值；若未來新增 screen，`switch` 中需同步新增 case，否則編譯器會發出 `-Wswitch` 警告 → 此為預期行為，有助維護

## Migration Plan

- 只修改 `status_bar.c` 的 `refresh_page_message()` 函數
- 不需 NVS 變更、不需 API 更動、不影響其他元件
- 修改後重新 build 即可
