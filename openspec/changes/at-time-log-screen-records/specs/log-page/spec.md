## CHANGED Requirements

### Requirement: Ring buffer 條目 tag 類型包含 AtTime

log-page 的 ring buffer 條目 tag SHALL 支援 AtTime 類型，並在畫面上顯示文字「定時」。

#### Scenario: AtTime 條目顯示
- **WHEN** 寫入 `LOG_TAG_AT` 的 log 條目
- **THEN** Log 頁以 `[定時]` 前綴顯示該條目

### Requirement: 可捲動彩色顯示

log-page SHALL 為 AtTime tag 提供可辨識的 INFO 顏色，且維持既有 level 覆蓋規則：WARN=橙、ERROR=紅。

#### Scenario: AtTime INFO 顏色
- **WHEN** 條目 tag 為 AtTime 且 level=INFO
- **THEN** 文字使用 AtTime 的 INFO 色

#### Scenario: AtTime WARN/ERROR 顏色
- **WHEN** 條目 tag 為 AtTime 且 level 為 WARN 或 ERROR
- **THEN** 分別使用全域 WARN/ERROR 覆蓋色
