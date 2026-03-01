## ADDED Requirements

### Requirement: Portal 頁面不新增 Settings 入口
`screen_portal.c` SHALL 維持既有 Portal 佈局，不新增 "Settings" 觸控按鈕。Settings 頁面入口由 Core2 硬體按鍵頁面輪詢提供。

#### Scenario: Portal 佈局維持不變
- **WHEN** 使用者進入 Portal 頁面
- **THEN** QR 與 AP 資訊區呈現與既有版本一致，無額外 Settings 按鈕

#### Scenario: Settings 入口不經 Portal 觸控
- **WHEN** 使用者要進入 Settings 頁面
- **THEN** 透過 Core2 硬體按鍵切換頁面，而非 Portal 頁面觸控入口
