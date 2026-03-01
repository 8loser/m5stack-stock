## ADDED Requirements

### Requirement: Vibration 區塊測試按鈕
HW Test 頁面 SHALL 提供 Vibration 區塊，含 3 個觸控按鈕：

- **Haptic**：按下後呼叫 `vibration_haptic()`（短震）
- **Alert**：按下後呼叫 `vibration_alert()`（長震）
- **Pulse 500ms**：按下後呼叫 `vibration_pulse(500)`（500ms 自訂震動）

#### Scenario: 按下 Haptic 按鈕
- **WHEN** 使用者點擊 "Haptic" 按鈕
- **THEN** `vibration_haptic()` 被呼叫，裝置產生短震動

#### Scenario: 按下 Alert 按鈕
- **WHEN** 使用者點擊 "Alert" 按鈕
- **THEN** `vibration_alert()` 被呼叫，裝置產生長震動

#### Scenario: 按下 Pulse 500ms 按鈕
- **WHEN** 使用者點擊 "Pulse 500ms" 按鈕
- **THEN** `vibration_pulse(500)` 被呼叫，裝置震動 500ms

### Requirement: Audio 區塊測試按鈕
HW Test 頁面 SHALL 提供 Audio 區塊，含 4 個觸控按鈕：

- **Beep 1kHz**：按下後呼叫 `audio_beep(1000, 200)`
- **Alert Up**：按下後呼叫 `audio_alert_up()`（漲停音）
- **Alert Down**：按下後呼叫 `audio_alert_down()`（跌停音）
- **Notify**：按下後呼叫 `audio_notify()`（AI 分析完成提示音）

#### Scenario: 按下 Beep 1kHz 按鈕
- **WHEN** 使用者點擊 "Beep 1kHz" 按鈕
- **THEN** `audio_beep(1000, 200)` 被呼叫，喇叭發出 1kHz 蜂鳴 200ms

#### Scenario: 按下 Alert Up 按鈕
- **WHEN** 使用者點擊 "Alert Up" 按鈕
- **THEN** `audio_alert_up()` 被呼叫，喇叭發出上揚雙音

#### Scenario: 按下 Alert Down 按鈕
- **WHEN** 使用者點擊 "Alert Down" 按鈕
- **THEN** `audio_alert_down()` 被呼叫，喇叭發出下降雙音

#### Scenario: 按下 Notify 按鈕
- **WHEN** 使用者點擊 "Notify" 按鈕
- **THEN** `audio_notify()` 被呼叫，喇叭發出提示音

### Requirement: 固定佈局無捲動
HW Test 頁面 SHALL 採用固定佈局，所有 7 個按鈕在 220px 可用高度內完整顯示，不需捲動。Vibration 區塊 3 個按鈕排列於同一行，Audio 區塊 4 個按鈕分兩行各 2 個。

#### Scenario: 所有按鈕可見
- **WHEN** 使用者進入 HW Test 頁面
- **THEN** Vibration 與 Audio 兩個區塊的所有 7 個按鈕均完整顯示，不需滑動
