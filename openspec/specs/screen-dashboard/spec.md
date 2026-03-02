# screen-dashboard Specification

## Purpose
TBD - created by syncing change add-hardware-test-page. Update Purpose after archive.

## Requirements
### Requirement: Dashboard 頁面提供 HW Test 入口
`screen_dashboard.c` SHALL 提供一個 "HW Test" 觸控按鈕。按下後呼叫 `ui_manager_switch_screen(SCREEN_HW_TEST)`。

#### Scenario: 由 Dashboard 進入 HW Test
- **WHEN** 使用者在 Dashboard 按下 "HW Test"
- **THEN** 畫面切換至 HW Test 頁面

#### Scenario: 不影響卡片顯示
- **WHEN** Dashboard 既有股票卡片正常顯示
- **THEN** 新增 HW Test 按鈕後，卡片區域仍可正常顯示與操作
