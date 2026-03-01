## ADDED Requirements

### Requirement: Portal 頁面提供 Settings 入口
`screen_portal.c` SHALL 在右側面板新增一個 "Settings" 觸控按鈕（`lv_btn`）。按下後呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`。

此為 Settings 頁面的唯一硬體入口，取代原 proposal 的 Dashboard 左鍵導航（已由 button-navigation-remap 佔用）。

#### Scenario: 進入 Settings
- **WHEN** 使用者在 Portal 頁面按下 "Settings" 觸控按鈕
- **THEN** 畫面切換至 Settings 頁面，roller 與 checkbox 顯示當前排程設定

#### Scenario: 不影響 Portal 其他功能
- **WHEN** Portal QR code / AP 資訊區正常顯示
- **THEN** Settings 按鈕加入不遮蔽 QR 區或 AP 資訊
