## 1. 實作

- [x] 1.1 修改 `status_bar.c` 的 `refresh_page_message()`，將 if/else 改為 `switch`，補全 `SCREEN_SETTINGS`（顯示 "Settings"）及 `SCREEN_DASHBOARD` 明確對應

## 2. 驗證

- [x] 2.1 Build 並燒錄，逐一切換至 Dashboard / Log / Info / Settings，確認 status bar 中央文字正確顯示各 screen 名稱
- [x] 2.2 確認 Settings 不再顯示 "Dashboard"
- [x] 2.3 確認 Portal 的 "Portal Setup - ..." 格式維持不變
