## Why

Portal 頁面視覺雜亂：冗余的狀態文字、QR code 後方醜陋的藍色背景方塊，以及 status bar
在 portal 頁面顯示過長的連線狀態字串，整體觀感粗糙。

## What Changes

- 移除 `screen_portal.c` 中的 `s_status_lbl`（「入口已啟動 - 掃描 QR 加入 AP」標籤）
- 移除 `left_panel` 的可見背景，改為透明容器，消除 QR code 後方的藍色方塊視覺
- Status bar (`SCREEN_PORTAL` case) 改為只顯示靜態字串 "Portal"，移除連線狀態動態文字

## Capabilities

### New Capabilities

（無）

### Modified Capabilities

- `portal-screen`: 移除冗余狀態標籤與 QR code 背景，改善頁面整體佈局
- `status-bar`: portal 頁面 status bar 訊息簡化為靜態 "Portal"

## Impact

- `components/ui/screens/screen_portal.c`：移除 `s_status_lbl` 相關程式碼；`left_panel` 改為透明；`update_portal_ui()` 簡化
- `components/ui/widgets/status_bar.c`：`refresh_page_message()` 中 `SCREEN_PORTAL` case 改為直接使用 `page_name = "Portal"`，刪除動態連線狀態邏輯
- 無 API、NVS、任務結構變更
