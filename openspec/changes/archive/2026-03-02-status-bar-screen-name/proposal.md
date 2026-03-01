## Why

Status bar 目前只顯示 WiFi 狀態和時間，使用者切換頁面後無法從 status bar 得知目前所在的 screen，需要依靠畫面內容自行判斷。在 5 個 screen 的導航流程中，加入 screen 名稱可提升方向感。

## What Changes

- Status bar 新增 screen 名稱標籤，顯示目前所在 screen 的英文短名稱
- `ui_manager_switch_screen()` 每次切換 screen 時同步更新 status bar 標題文字
- 各 screen 對應的顯示名稱在 `ui_manager.c` 內以靜態對照表定義

## Capabilities

### New Capabilities

（無）

### Modified Capabilities

- `ui-manager`：`ui_manager_switch_screen()` 新增行為——切換 screen 時須更新 status bar 上的 screen 名稱標籤

## Impact

- `components/ui/ui_manager.c`：修改 `switch_screen()` 邏輯，新增 status bar 標題更新呼叫
- `components/ui/include/ui_manager.h`：如需對外暴露 screen 名稱查詢，可能新增 getter
- `components/ui/CMakeLists.txt`：無變更預期
- 不影響其他元件，無 API breaking change
