## Context

`screen_log.c` 目前是 stub，只有標題 label 和提示文字。現有狀態：

- `ui_manager.h`：`SCREEN_LOG = 2`、`SCREEN_INFO = 3` 已定義
- `ui_manager.c`：`s_nav_screens[]` 已含 `SCREEN_LOG`、`screen_log_create()` 已掛入初始化
- `status_bar.c`：`refresh_page_message()` 已處理 `SCREEN_LOG → "Log"` 分支

尚未實作：
- `screen_log.h`：公開 API（`log_tag_t`、`log_level_t`、`screen_log_push()`、`screen_log_refresh()`）
- `screen_log.c`：ring buffer、12 個靜態 label、顏色渲染
- `ui_manager.h/.c`：4 個 log wrapper API（`ui_manager_log_stock/wifi/ai/sys`）
- `ui_manager.c`：切換到 `SCREEN_LOG` 時呼叫 `screen_log_refresh()`
- `main.c`：WiFi log、啟動 SYS log、Quote 批次摘要、AI result queue 消費

## Goals / Non-Goals

**Goals:**
- Thread-safe ring buffer（32 條），任何 FreeRTOS task 均可寫入
- 12 個靜態 label，最新條目在最上方
- 4 種 tag（STOCK / WIFI / AI / SYS）× 3 種 level（INFO / WARN / ERROR），各有對應顏色
- `ui_manager` 提供 4 個 log wrapper，供跨元件呼叫
- `main.c` 在 WiFi 狀態轉換、啟動、Quote 更新、AI 結果時寫入對應 log

**Non-Goals:**
- 可捲動列表（只顯示最新 12 條）
- 日誌持久化到 NVS
- 依 tag / level 篩選
- 每條記錄附時間戳（空間不足）

## Decisions

### D1：Ring Buffer — 靜態陣列 + portMUX

```c
#define LOG_RING_SIZE  32
#define LOG_MSG_MAX    56

typedef struct {
    log_tag_t   tag;
    log_level_t level;
    char        msg[LOG_MSG_MAX];
} log_entry_t;

static log_entry_t  s_ring[LOG_RING_SIZE];
static int          s_head  = 0;   /* 下一個寫入位置 */
static int          s_count = 0;   /* 已填入條數（上限 LOG_RING_SIZE） */
static portMUX_TYPE s_mux   = portMUX_INITIALIZER_UNLOCKED;
```

選用 `portMUX` 而非 FreeRTOS mutex：
- 寫入路徑是 O(1) struct copy，臨界區極短（< 10 µs）
- 不需要等待（mutex 可能阻塞 task），不需要初始化呼叫
- 替代方案：`SemaphoreHandle_t mutex`——較重，需初始化，但功能等效；此場景不必要

記憶體：32 × 64 bytes = 2 KB（internal RAM，可接受）

### D2：12 個靜態 LVGL Label

在 `screen_log_create()` 時預先配置 12 個 `lv_obj_t *s_labels[12]`。
`screen_log_refresh()` 時，從最新往最舊迭代 ring buffer，對每個 label 呼叫：
- `lv_label_set_text()`
- `lv_obj_set_style_text_color()` 依 tag + level

超過 12 條的舊條目保留在 ring buffer，未來若需捲動可擴充。

替代方案：`lv_list` 或動態建立 label——複雜度高，易造成 LVGL heap 碎片化；靜態 label 更安全。

Label 幾何：`y = 28 + i * 17`，`font = montserrat_12`，左對齊，padding 4px。

### D3：顏色方案

tag 決定基本顏色（INFO level）；level WARN/ERROR 強制覆蓋為統一警示色：

| Tag   | INFO 色       | WARN 色       | ERROR 色      |
|-------|---------------|---------------|---------------|
| STOCK | 0x4FC3F7（藍）| 0xFFB74D（橙）| 0xEF5350（紅）|
| WIFI  | 0x81C784（綠）| 0xFFB74D（橙）| 0xEF5350（紅）|
| AI    | 0xCE93D8（紫）| 0xFFB74D（橙）| 0xEF5350（紅）|
| SYS   | 0xB0BEC5（灰）| 0xFFB74D（橙）| 0xEF5350（紅）|

### D4：screen_log_refresh() 在 switch_screen 中呼叫

`ui_manager_switch_screen()` 已有 per-screen side effect 模式（Portal open/close）。新增：

```c
if (id == SCREEN_LOG) {
    extern void screen_log_refresh(void);
    screen_log_refresh();
}
```

在 `xSemaphoreTakeRecursive(s_ui_mutex)` 持有期間呼叫，LVGL 呼叫安全。

### D5：Quote 批次摘要（main.c）

`g_quote_queue` 每輪可能接收多筆報價。逐筆 log 會快速填滿 ring buffer。
改為：drain queue 後統計接收筆數，push 一筆摘要：`"Updated N stocks"` / `LOG_TAG_STOCK / LOG_LEVEL_INFO`。

### D6：AI Result Queue 消費（main.c）

`g_ai_result_queue` 目前在 main loop 完全未消費。
新增非阻塞 `xQueueReceive(g_ai_result_queue, &ai_result, 0)`，消費後：
- 呼叫 `ui_manager_update_ai_result()`（若有實作）或直接 log
- 呼叫 `ui_manager_log_ai()` 傳入前 50 個字元的分析摘要

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| portMUX 短暫停用中斷 | 寫入路徑 O(1)，< 10 µs，不影響系統即時性 |
| 12 個 label 不夠顯示所有事件 | Ring buffer 保留 32 條，日後可擴充捲動 |
| LVGL label 更新未持有 mutex | `screen_log_refresh()` 只在 `switch_screen` 的 mutex 持有期間呼叫 |
| msg 欄位 56 bytes 不足 | wrapper 使用 `snprintf` 截斷，不會 overflow |
| AI result struct 大小未知 | 需確認 `ai_analysis_result_t` 大小，stack 宣告需注意 |

## Migration Plan

1. 建立 `screen_log.h`（enums + API 宣告）
2. 重寫 `screen_log.c`（ring buffer + 12 labels + refresh）
3. 在 `ui_manager_switch_screen()` 新增 `screen_log_refresh()` 呼叫
4. 在 `ui_manager.h` 新增 4 個 log API 宣告
5. 在 `ui_manager.c` 實作 4 個 log wrapper
6. 更新 `main.c`：WiFi log、SYS 啟動 log、Quote 批次摘要、AI queue 消費

無 NVS 遷移，無資料流失風險。回滾：`git revert` 單一 commit。
