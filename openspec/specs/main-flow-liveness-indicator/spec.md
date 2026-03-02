# main-flow-liveness-indicator Specification

## Purpose
TBD - created by archiving change add-main-flow-heartbeat-indicator. Update Purpose after archive.
## Requirements
### Requirement: 狀態列顯示可視化主流程活性
系統 SHALL 在共用 status bar 提供可視化主流程活性指標，讓使用者可快速判斷裝置是否疑似當機。

#### Scenario: 顯示位置固定於時間前方
- **WHEN** status bar 完成布局
- **THEN** 活性指標位於時間文字前方，且不因頁面切換消失

### Requirement: 主流程活性異常時顯示警示符號
系統 SHALL 在主流程活性判定失敗時將指標切換為 `!`，並停止正常心跳動畫。

#### Scenario: 主流程逾時
- **WHEN** 主流程心跳判定為非存活
- **THEN** status bar 指標顯示 `!` 且停閃

### Requirement: 正常狀態使用雙擊節奏
系統 SHALL 在主流程活性正常時使用雙擊節奏顯示心跳符號。

#### Scenario: 主流程存活
- **WHEN** 主流程心跳判定為存活
- **THEN** status bar 指標以雙擊節奏閃爍

