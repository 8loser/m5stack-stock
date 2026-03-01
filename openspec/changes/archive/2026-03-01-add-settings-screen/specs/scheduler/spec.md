## ADDED Requirements

### Requirement: 非開市固定跳過報價抓取
排程器 SHALL 在非市場開市時間固定跳過報價抓取，不由 UI 參數切換。

#### Scenario: 非開市時間觸發報價排程
- **WHEN** quote timer 到期且 `rtc_bm8563_is_market_open()` 為 false
- **THEN** 本次報價抓取被跳過
