## CHANGED Requirements

### Requirement: 縮短 Portal drain timeout

`PORTAL_NET_DRAIN_TIMEOUT_MS` SHALL 從 `HTTP_TIMEOUT_MS + 5000`（20s）縮短為 2000~3000ms。主動 abort 後 drain 只需等待 task cleanup 退出，不再需要等待 HTTP 自然超時。

#### Scenario: drain 在新 timeout 內完成
- **WHEN** 按中鍵進入 Portal，telegram 和 fetch 被主動 abort
- **THEN** drain 在 < 1s 完成，portal 啟動，使用者體感進入時間 < 2s

#### Scenario: drain 在新 timeout 內未完成（異常）
- **WHEN** abort 後 task 未在 timeout 內退出
- **THEN** 記錄 warning log，portal 仍然啟動（non-fatal，現有行為不變）
