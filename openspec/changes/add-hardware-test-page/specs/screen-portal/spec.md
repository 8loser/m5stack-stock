## ADDED Requirements

### Requirement: Portal 頁面提供 HW Test 入口
`screen_portal.c` SHALL 在右側面板新增一個 "HW Test" 觸控按鈕（`lv_btn`）。按下後呼叫 `ui_manager_switch_screen(SCREEN_HW_TEST)`。

此為 HW Test 頁面的唯一入口，與 Settings 入口模式一致（Portal 作為功能性頁面入口中樞）。

#### Scenario: 進入 HW Test
- **WHEN** 使用者在 Portal 頁面按下 "HW Test" 觸控按鈕
- **THEN** 畫面切換至 HW Test 頁面，顯示 Vibration 與 Audio 測試區塊

#### Scenario: 不影響 Portal 其他功能
- **WHEN** Portal QR code / AP 資訊區正常顯示
- **THEN** HW Test 按鈕加入後不遮蔽 QR 區或 AP 資訊，與 Settings 按鈕合理共存
