## ADDED Requirements

### Requirement: Ring buffer 儲存事件歷史
系統 SHALL 維護一個 32 條容量的環形緩衝區，以先進先出方式記錄應用層事件。當緩衝區滿時，最舊條目 SHALL 被覆蓋。每條條目 SHALL 包含 tag（STOCK/WIFI/AI/SYS）、level（INFO/WARN/ERROR）、日期時間字串（`YYYY-MM-DD HH:MM:SS`）與最多 55 字元的訊息文字。

#### Scenario: 正常寫入
- **WHEN** 呼叫 `screen_log_push(tag, level, msg)`
- **THEN** 條目被加入 ring buffer，count 遞增（上限 32），不阻塞呼叫 task

#### Scenario: Buffer 滿時覆寫
- **WHEN** ring buffer 已有 32 條，再次呼叫 `screen_log_push()`
- **THEN** 最舊條目被覆蓋，head index 前進，顯示最新 12 條不含被覆蓋的舊條目

#### Scenario: 訊息截斷
- **WHEN** 傳入 msg 超過 55 字元
- **THEN** 自動截斷至 55 字元，不發生 buffer overflow

### Requirement: Thread-safe 寫入
`screen_log_push()` SHALL 使用 portMUX critical section 保護 ring buffer 讀寫，確保多個 FreeRTOS task 同時呼叫時不發生 race condition。

#### Scenario: 多 task 同時寫入
- **WHEN** wifi_manager task 與 main task 同時呼叫 `screen_log_push()`
- **THEN** 兩條記錄均被正確寫入，無資料損毀

### Requirement: 可捲動彩色顯示
Log 頁面 SHALL 以可垂直捲動清單顯示 ring buffer 事件，最新條目在最上方。每條顯示格式為懸掛縮排：第一行 `[TAG] YYYY-MM-DD HH:MM:SS <message...>`，續行保留縮排。顏色依 tag 與 level 決定：
- STOCK INFO=0x4FC3F7（藍）、WIFI INFO=0x81C784（綠）、AI INFO=0xCE93D8（紫）、SYS INFO=0xB0BEC5（灰）
- WARN 強制覆蓋為 0xFFB74D（橙）、ERROR 強制覆蓋為 0xEF5350（紅），不論 tag

#### Scenario: 頁面切換後刷新
- **WHEN** 使用者切換至 Log 頁面（`ui_manager_switch_screen(SCREEN_LOG)`）
- **THEN** Log 清單依 ring buffer 最新內容更新，最新條目顯示在最上方

#### Scenario: 長訊息換行
- **WHEN** 單條事件訊息長度超過一行可顯示寬度
- **THEN** 事件文字續行顯示並保留固定縮排

#### Scenario: WARN level 顏色
- **WHEN** 條目 level 為 WARN（任何 tag）
- **THEN** 文字顏色為 0xFFB74D，不論 tag 的 INFO 顏色

#### Scenario: ERROR level 顏色
- **WHEN** 條目 level 為 ERROR（任何 tag）
- **THEN** 文字顏色為 0xEF5350，不論 tag 的 INFO 顏色

### Requirement: screen_log_refresh 更新顯示
系統 SHALL 提供 `screen_log_refresh()` 函數，在持有 LVGL mutex 的情況下更新清單內所有可視事件文字與顏色。此函數 SHALL 只從 UI context（持有 g_ui_mutex）呼叫。

#### Scenario: 切換到 Log 頁面時自動刷新
- **WHEN** `ui_manager_switch_screen(SCREEN_LOG)` 被呼叫
- **THEN** `screen_log_refresh()` 在 mutex 持有期間被呼叫，label 立即反映最新 ring buffer 內容
