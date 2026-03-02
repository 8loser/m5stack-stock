## REMOVED Requirements

### Requirement: Portal 狀態標籤
**Reason**: `s_status_lbl`（顯示「入口已啟動 - 掃描 QR 加入 AP」）資訊已由 status bar 涵蓋，屬於冗余 UI 元素，對用戶無額外資訊價值。
**Migration**: 不需替代；status bar 標題已足夠表達目前頁面狀態。

## MODIFIED Requirements

### Requirement: Portal 頁面不新增 Settings 入口
`screen_portal.c` SHALL 維持既有 Portal 佈局，不新增 "Settings" 觸控按鈕。Settings 頁面入口由 Core2 硬體按鍵頁面輪詢提供。QR code SHALL 直接掛載於 `s_qr_area`，不使用帶有可見背景的中間容器（`left_panel`）。

#### Scenario: Portal 佈局維持不變
- **WHEN** 使用者進入 Portal 頁面
- **THEN** QR 與 AP 資訊區正常呈現，無額外 Settings 按鈕，QR code 後方無藍色背景方塊

#### Scenario: Settings 入口不經 Portal 觸控
- **WHEN** 使用者要進入 Settings 頁面
- **THEN** 透過 Core2 硬體按鍵切換頁面，而非 Portal 頁面觸控入口

#### Scenario: QR code 無多餘背景
- **WHEN** Portal AP 已啟動且 QR code 顯示
- **THEN** QR code 直接顯示於螢幕背景上，無藍色面板疊底
