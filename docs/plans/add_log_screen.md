# Plan: 新增 Log 頁面（SCREEN_LOG）

## Context

目前 UI 只有 Dashboard（股票行情）和 Portal（WiFi 配網）兩頁，使用者在裝置上無法查看系統活動歷史。
新增第三頁 Log，以環形緩衝區記錄 4 種應用層事件（股票更新、WiFi 狀態、AI 分析、系統事件），並在 Core2 螢幕上顯示。
導航：左鍵（BtnA）在 Dashboard ↔ Log 之間切換，右鍵永遠進 Portal。

---

## 導航規則（變更後）

| 頁面 | 左鍵（btn=0） | 中鍵（btn=1） | 右鍵（btn=2） |
|------|-------------|-------------|-------------|
| Dashboard | **進入 Log**（原無動作） | 手動刷新股價（不變） | 進入 Portal（不變） |
| Log | **返回 Dashboard** | - | 進入 Portal |
| Portal | 返回 Dashboard（不變） | - | - |

---

## Log 頁面顯示範例

```
10:35 [STK] Quotes: 5 ok, 0 err
10:34 [WFI] Connected: 192.168.1.5
10:30 [AI ] analysis done: HOLD(72)
10:15 [AI ] analysis failed
10:00 [SYS] System booted
```

- 最新條目在最上方（倒序）
- 最多顯示 12 行、保留 32 條歷史
- 顏色：STK=藍、WFI=綠、AI=橙、SYS=灰；WARN=黃、ERROR=紅

---

## Task 清單

- [ ] **T1** 新建 `components/ui/include/screen_log.h`
  - 定義 `log_tag_t`（STOCK/WIFI/AI/SYS）
  - 定義 `log_level_t`（INFO/WARN/ERROR）
  - 宣告 `screen_log_create()`、`screen_log_push()`、`screen_log_refresh()`

- [ ] **T2** 新建 `components/ui/screens/screen_log.c`
  - 靜態 ring buffer（32 條，portMUX 保護）
  - `screen_log_push()`：thread-safe 寫入，可從任何 task 呼叫
  - `screen_log_create()`：12 個靜態 lv_label，背景 `0x1A1A2E`
  - `screen_log_refresh()`：snapshot ring buffer → 更新 label 文字 + 顏色（需持有 ui_mutex）

- [ ] **T3** 修改 `components/ui/include/ui_manager.h`
  - 新增 `SCREEN_LOG = 2` 到 `screen_id_t` enum
  - 新增 4 個 log API 宣告（thread-safe）

- [ ] **T4** 修改 `components/ui/ui_manager.c`（5 處）
  - `s_screens[2]` → `s_screens[3]`
  - init 加 `screen_log_create()`
  - `switch_screen` guard `>= 2` → `>= 3`；進入 LOG 時呼叫 `screen_log_refresh()`
  - `handle_hw_button` 新增 Dashboard 左鍵 + `SCREEN_LOG` case
  - 實作 4 個 `ui_manager_log_*()` 函式

- [ ] **T5** 修改 `components/ui/widgets/status_bar.c`
  - `refresh_page_message()` 加入 `SCREEN_LOG` → 顯示 `"Event Log"`

- [ ] **T6** 修改 `components/ui/CMakeLists.txt`
  - SRCS 加 `"screens/screen_log.c"`

- [ ] **T7** 修改 `main/main.c`
  - WiFi callback 加 `ui_manager_log_wifi()`
  - 啟動完成加 `ui_manager_log_sys("System booted")`
  - Quote 消費改為批次摘要後記一條（避免塞爆 ring buffer）
  - 補上 AI result queue 消費 + `ui_manager_log_ai()`

- [ ] **T8** Build & Flash 驗證

---

## 關鍵實作細節

### Ring Buffer 結構

```c
#define LOG_RING_SIZE   32
#define LOG_MSG_LEN     48

typedef struct {
    uint32_t    timestamp_s;  /* time(NULL) */
    log_tag_t   tag;
    log_level_t level;
    char        msg[LOG_MSG_LEN];
} log_entry_t;

static log_entry_t  s_ring[LOG_RING_SIZE];
static int          s_head  = 0;
static int          s_count = 0;
static portMUX_TYPE s_ring_mux = portMUX_INITIALIZER_UNLOCKED;
```

### LVGL UI 佈局

```c
#define LOG_VISIBLE_ROWS 12
#define LOG_ROW_HEIGHT   18
#define LOG_START_Y      22   /* status bar 20px + 2px padding */
```

- 字型：`lv_font_montserrat_10`（ui_compat.h alias，安全）
- 格式：`HH:MM [STK] msg`（tag 縮寫：STK/WFI/AI/SYS）
- `lv_label_set_long_mode(label, LV_LABEL_LONG_DOT)` 截長文字

### 顏色對應

| 條件 | 顏色 |
|------|------|
| INFO + STOCK | 藍 `0x64B5F6` |
| INFO + WIFI | 綠 `0x81C784` |
| INFO + AI | 橙 `0xFFB74D` |
| INFO + SYS | 灰 `0xAAAAAA` |
| WARN（任何 tag）| 黃 `0xFFCC00` |
| ERROR（任何 tag）| 紅 `0xFF5252` |

### ui_manager.h 新增 API

```c
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_PORTAL    = 1,
    SCREEN_LOG       = 2,   /* 新增 */
} screen_id_t;

/* thread-safe，可從任何 task 呼叫，無需 ui_mutex */
void ui_manager_log_stock(bool success, const char *msg);
void ui_manager_log_wifi(int state, const char *ip);
void ui_manager_log_ai(bool success, int signal);
void ui_manager_log_sys(const char *msg);
```

### main.c Quote 批次摘要（避免塞爆 ring buffer）

```c
int updated = 0, failed = 0;
while (xQueueReceive(g_quote_queue, &quote, 0) == pdTRUE) {
    ui_manager_update_quote(&quote);
    if (quote.is_valid) updated++;
    else failed++;
}
if (updated > 0 || failed > 0) {
    char msg[40];
    snprintf(msg, sizeof(msg), "Quotes: %d ok, %d err", updated, failed);
    ui_manager_log_stock(failed == 0, msg);
}
```

### main.c AI result 消費（目前主迴圈缺失）

```c
ai_analysis_result_t ai_result;
if (xQueueReceive(g_ai_result_queue, &ai_result, 0) == pdTRUE) {
    bool ok = (ai_result.error_code == ESP_OK);
    ui_manager_log_ai(ok, (int)ai_result.signal);
}
```

---

## 修改的檔案清單

| 檔案 | 動作 |
|------|------|
| `components/ui/include/screen_log.h` | 新增 |
| `components/ui/screens/screen_log.c` | 新增 |
| `components/ui/include/ui_manager.h` | 修改 |
| `components/ui/ui_manager.c` | 修改 |
| `components/ui/widgets/status_bar.c` | 修改 |
| `components/ui/CMakeLists.txt` | 修改 |
| `main/main.c` | 修改 |

---

## 關鍵注意事項

- `screen_log_push`（portMUX）可從任何 task 呼叫；`screen_log_refresh`（lv_* 呼叫）需持有 ui_mutex
- `lv_font_montserrat_10` 在 `ui_compat.h:19` 定義為 alias，直接用
- `ai_analysis_result_t` 無 symbol 欄位，AI log 用 signal（int）呈現
- `status_bar.c` 的 `refresh_page_message` 原為 if-else（else = Dashboard），改成 else-if 三段
- `ai_result_queue` 在 main.c 主迴圈目前從未消費，T7 順便補上

---

## Verification

1. `./flash.sh --build-only` — 無 error/warning
2. 燒錄後測試導航循環：Dashboard 左鍵 → Log → 左鍵 → Dashboard
3. Log 頁面應顯示 `SYS: System booted` 最早一條
4. 等待 WiFi 連線，Log 頁面自動更新 WFI 條目
5. 等待 60s 排程觸發，應看到 STK 條目
6. Portal 進出行為不變（右鍵進、左鍵出）
7. monitor 無 mutex timeout / heap 不足 / Guru Meditation Error
