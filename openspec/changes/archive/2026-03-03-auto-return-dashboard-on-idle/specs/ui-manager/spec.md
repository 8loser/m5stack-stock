## ADDED Requirements

### Requirement: 非首頁畫面閒置超時自動返回 Dashboard
`ui_manager` SHALL 在目前畫面為非 `SCREEN_DASHBOARD` 且非 `SCREEN_PORTAL` 時，監測觸控閒置時間；若連續 10 秒無觸控按下事件，系統 SHALL 自動切換至 `SCREEN_DASHBOARD`。

#### Scenario: 非首頁畫面閒置超時返回
- **WHEN** 使用者停留在 `SCREEN_INFO`（或 `SCREEN_LOG` / `SCREEN_SETTINGS` / `SCREEN_HW_TEST`）且 10 秒內沒有觸控按下
- **THEN** `ui_manager` 會呼叫 `ui_manager_switch_screen(SCREEN_DASHBOARD)`

#### Scenario: Dashboard 不觸發閒置返回
- **WHEN** 使用者停留在 `SCREEN_DASHBOARD` 超過 10 秒且無觸控
- **THEN** 系統 SHALL NOT 因閒置邏輯觸發切頁

#### Scenario: Portal 不觸發閒置返回
- **WHEN** 使用者停留在 `SCREEN_PORTAL` 超過 10 秒且無觸控
- **THEN** 系統 SHALL NOT 因閒置邏輯觸發切頁

### Requirement: 觸控按下事件重置閒置倒數
`ui_manager` SHALL 在任意觸控按下事件發生時重置閒置倒數；此條件包含一般觸控區與底部虛擬按鍵觸控區。

#### Scenario: 一般觸控重置倒數
- **WHEN** 使用者在非首頁畫面發生一般觸控按下
- **THEN** 閒置計時基準更新，10 秒倒數重新開始

#### Scenario: 底部虛擬按鍵觸控重置倒數
- **WHEN** 使用者在底部虛擬按鍵區發生觸控按下
- **THEN** 閒置計時基準更新，10 秒倒數重新開始

### Requirement: 熄屏期間暫停閒置計時
當螢幕處於關閉狀態時，閒置返回邏輯 SHALL 暫停計時與判定；僅在亮屏期間進行超時檢查。

#### Scenario: 熄屏期間不計時
- **WHEN** 使用者在非首頁畫面熄屏並維持超過 10 秒
- **THEN** 系統 SHALL NOT 在熄屏期間觸發自動返回

#### Scenario: 亮屏後恢復檢查
- **WHEN** 使用者重新亮屏且仍在非首頁畫面
- **THEN** 系統恢復閒置倒數檢查，不以熄屏期間累積時間直接判定超時

### Requirement: 亮屏後顯示 Dashboard
系統在螢幕關閉期間 SHALL 預先切換至 `SCREEN_DASHBOARD`，使螢幕由關閉轉為開啟時直接顯示主監看頁，且不先短暫顯示關閉前頁面。

#### Scenario: 非首頁熄屏再亮屏回 Dashboard
- **WHEN** 使用者在 `SCREEN_INFO`（或其他非 Dashboard 畫面）熄屏後再亮屏
- **THEN** 亮屏第一時間顯示 `SCREEN_DASHBOARD`，不先顯示先前頁面

#### Scenario: 已在 Dashboard 亮屏維持 Dashboard
- **WHEN** 使用者在 `SCREEN_DASHBOARD` 熄屏後再亮屏
- **THEN** 畫面維持在 `SCREEN_DASHBOARD`
