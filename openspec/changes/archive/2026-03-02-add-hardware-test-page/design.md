## Context

`screen_hw_test.c` 目前不存在。現有基礎：

- `ui_manager.h`：目前 enum 末尾為 `SCREEN_INFO = 3`（或含 `add-settings-screen` 後為 `SCREEN_SETTINGS = 4`）
- `ui_manager.c`：`s_nav_screens[] = {SCREEN_DASHBOARD, SCREEN_LOG, SCREEN_INFO}`
- HAL API 已就緒：

| HAL 函數 | 說明 |
|----------|------|
| `vibration_haptic()` | 短震（觸控回饋用，~50ms）|
| `vibration_alert()` | 長震（價格警報用，~500ms）|
| `vibration_pulse(ms)` | 自訂時長震動 |
| `audio_beep(hz, ms)` | 單音蜂鳴 |
| `audio_alert_up()` | 上揚雙音（漲停）|
| `audio_alert_down()` | 下降雙音（跌停）|
| `audio_notify()` | AI 分析完成提示音 |

注意：`vibration_haptic()` 已在 `lvgl_touch_cb` 中於所有觸控按下時呼叫，HW Test 頁的 vibration 按鈕按下會觸發「雙重震動」（touch haptic + 測試震動），在測試頁語境下可接受。

## Goals / Non-Goals

**Goals:**
- 7 個測試按鈕（Vibration × 3、Audio × 4），按下即觸發對應 HAL 呼叫
- 固定佈局（無需捲動）
- 從 Portal 觸控按鈕進入，與 Settings 入口模式一致
- 不需返回按鈕（使用現有 btn=1 → Portal 全域語義）

**Non-Goals:**
- 調整震動時長或音頻參數
- 連續 / 循環播放
- 測試結果回饋顯示（無感測器讀回）
- 加入 `s_nav_screens[]` 輪詢（功能性頁面）

## Decisions

### D1：SCREEN_HW_TEST 位置

加入 enum 末尾：若 `add-settings-screen` 已套用則為 `SCREEN_HW_TEST = 5`，否則為 `SCREEN_HW_TEST = 4`。設計上不依賴具體數值，僅要求在 `SCREEN_COUNT` 之前。

### D2：導航入口——Portal 觸控按鈕（同 Settings 模式）

原 proposal 提及「從 Settings / Portal 頁面可導航進入」。與 `add-settings-screen` 一致，在 Portal 右側面板新增 "HW Test" 觸控按鈕。硬體按鍵不做額外路由（btn=1 全域語義為 Portal toggle）。

### D3：頁面佈局（固定，無捲動）

螢幕可用高度 220px，預計佈局：

```
y=22   "HW TEST"            (montserrat_20, 置中)
y=44   "VIBRATION"          (montserrat_12, 顏色 0x4FC3F7)
y=60   [Haptic][Alert][Pulse 500ms]   3 buttons × 95px wide, h=36, gap=5
y=104  "AUDIO"              (montserrat_12, 顏色 0xCE93D8)
y=120  [Beep 1kHz][Alert Up]          2 buttons × 147px wide, h=36, gap=6
y=162  [Alert Down][Notify]           2 buttons × 147px wide, h=36, gap=6
```

總高度 ≈ 198px，無需捲動。

### D4：HAL 呼叫在 LVGL 事件回調中執行

7 個按鈕的 `LV_EVENT_CLICKED` handler 直接呼叫 HAL 函數：
- `vibration_haptic/alert/pulse(500)` 均為非阻塞（使用 LEDC PWM，設定後立即返回）
- `audio_beep(1000, 200)` / `audio_alert_up/down()` / `audio_notify()` 為同步播音

Audio 函數若阻塞（等待播音完成）會短暫凍結 LVGL（200ms 以內）。對測試頁可接受；若實際阻塞時間超過 300ms，可考慮建立獨立 task 播音，但保留為後續優化。

替代方案：Audio 按鈕建立一次性 FreeRTOS task 播音——避免 UI 凍結，但複雜度高；測試頁不必要。

### D5：按鈕顏色區分區塊

- Vibration 按鈕：背景 `0x1565C0`（深藍），文字白色
- Audio 按鈕：背景 `0x4A148C`（深紫），文字白色

兩個 section label 顏色與 `add-info-screen`、`add-log-screen` 一致（Vibration=0x4FC3F7、Audio=0xCE93D8）。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| Audio 同步播音造成 UI 凍結 | 測試頁語境下接受；凍結 < 500ms；紀錄為 known limitation |
| 雙重震動（touch haptic + 測試震動）| 測試頁語境下符合預期，使用者目的就是驗證震動 |
| SCREEN_HW_TEST 數值依套用順序不同 | 不直接使用數值，只用 enum 名稱；SCREEN_COUNT 自動正確 |

## Migration Plan

1. `ui_manager.h`：加入 `SCREEN_HW_TEST` enum
2. 新增 `components/ui/screens/screen_hw_test.c`（7 個按鈕 + HAL 呼叫）
3. `ui_manager.c`：init 加 `screen_hw_test_create()`
4. `screen_portal.c`：右側面板新增 "HW Test" 按鈕
5. `CMakeLists.txt`：SRCS 加 `screen_hw_test.c`
6. 驗證：Portal → HW Test → 逐一測試 7 個按鈕

無 NVS 操作，無資料遷移風險。回滾：`git revert`。
