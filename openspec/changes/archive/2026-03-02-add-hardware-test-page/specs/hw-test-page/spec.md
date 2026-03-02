## ADDED Requirements

### Requirement: HW Test 頁面提供最小硬體測試按鈕
HW Test 頁面 SHALL 提供兩個測試區塊，並各自只有一個按鈕：

- **Vibration** 區塊：`Vibrate` 按鈕（呼叫 `vibration_alert()`）
- **Audio** 區塊：`Beep` 按鈕（播放 4 個短音）

#### Scenario: 按下 Vibrate
- **WHEN** 使用者點擊 "Vibrate" 按鈕
- **THEN** `vibration_alert()` 被呼叫，裝置產生長震動

#### Scenario: 按下 Beep
- **WHEN** 使用者點擊 "Beep" 按鈕
- **THEN** 裝置播放 4 個短音，且自動停止

### Requirement: Beep 使用四個不同音調
`Beep` 音效 SHALL 由四個短音構成，且四個短音使用不同音調。

#### Scenario: Beep 音調
- **WHEN** 使用者點擊 "Beep"
- **THEN** 依序播放由低到高的 4 個短音

### Requirement: 版面收斂與垂直置中
HW Test 頁面 SHALL 不顯示頁面主標題（例如 "HW TEST"），內容區塊需垂直置中，且整頁不需捲動。

#### Scenario: 進入頁面
- **WHEN** 使用者進入 HW Test 頁面
- **THEN** 可見 `Vibrate` 與 `Beep` 兩個按鈕，內容位於畫面中段且無需滑動
