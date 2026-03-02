## Context

Dashboard 的三個問題根源各自獨立：
1. **padding clip**：LVGL `lv_obj` 預設 padding=8px，card 高度 32px → content area 只有 16px，14px 字型從 y=4 開始佔到 y=18，被 clip 在 y=16。
2. **next time 缺失**：scheduler 使用 FreeRTOS `TimerHandle_t`，有精確的下次觸發時間，但未對外暴露 API。
3. **固定 5 row**：`screen_dashboard_create()` 在 `ui_manager_init()` 時呼叫，此時 `storage_init()` 尚未執行（開機順序：`board_init → ui_manager_init → storage_init`），無法在 create 時讀 NVS 股票數。

## Goals / Non-Goals

**Goals:**
- 股票代碼文字完整顯示（無截斷）
- Dashboard 底部顯示下次報價時間（絕對時間 HH:MM:SS）
- 已設定 n 支股票時只顯示 n 個 row

**Non-Goals:**
- next time 倒數計時（需每秒 refresh，超出此次範圍）
- 修改開機動畫或 loading 狀態

## Decisions

### D1：移除 card padding
直接在 `screen_dashboard_create()` 的 card 建立迴圈中加：
```c
lv_obj_set_style_pad_all(s_cards[i], 0, 0);
```
不調整 card 高度或字型大小，content area 直接等於 32px 即可容下兩行 14px 文字。

### D2：next time 來源 — scheduler API（採用）vs. 本地計算

**採用**：`scheduler_get_seconds_to_next_quote()` 回傳 `uint32_t`（剩餘秒數），內部用：
```c
TickType_t expiry = xTimerGetExpiryTime(s_quote_timer);
TickType_t now    = xTaskGetTickCount();
TickType_t diff   = (expiry > now) ? (expiry - now) : 0;
return (uint32_t)(diff / configTICK_RATE_HZ);
```
**捨棄**：main.c 自行計算 `now + interval`。問題：timer 可能因 `scheduler_trigger_quote_now()` 提前觸發，本地計算不準確。

呼叫時機：`ui_manager_update_quote()` 每次收到報價時，同步呼叫 scheduler 取得剩餘秒數，換算成絕對時間後更新 `s_next_label`。

### D3：動態 card 數 + 自動輪播分頁（採用）

**採用**：`s_card_list` 捲動容器 + LVGL Timer 定期自動翻頁。

```
s_card_list: lv_obj_t
  pos:  (4, 24)
  size: (LCD_WIDTH - 8, 182)   ← 剛好到 footer 上方
  style: bg_opa=TRANSP, border_width=0, pad_all=0
  scrollable: 開啟（支援手動滑動）
```

Card 在容器內以 `lv_obj_set_pos(card, 0, i * 36)` 排列。`MAX_CARDS` 改為 `MAX_STOCK_COUNT`（10）。

**自動翻頁**：建立 LVGL Timer（`lv_timer_create`），週期為 `DASHBOARD_PAGE_FLIP_S`（定義在 `app_config.h`，預設 5 秒）。每次觸發：

```c
static int s_cur_page = 0;
int total_pages = (s_card_count + 4) / 5;   // ceil(count / 5)
s_cur_page = (s_cur_page + 1) % total_pages;
lv_obj_scroll_to_y(s_card_list, s_cur_page * 5 * 36, LV_ANIM_ON);
```

- 5 支以內：total_pages=1，timer 觸發但 scroll 位置不變，無視覺效果
- 6-10 支：自動以平滑動畫滑至下一批，回捲時同樣帶動畫

**延遲初始化**：card 全部 hidden，`screen_dashboard_set_card_count(uint8_t n)` 由 main.c 在 `storage_init()` 後呼叫（持 g_ui_mutex）。同時重設 `s_cur_page = 0`。

**空白提示**：在 `s_card_list` 內建立 `s_empty_label`，居中顯示 `"No stocks configured.\nGo to Settings to add."`；count=0 時顯示，count≥1 時 hidden。

**捨棄**：手動捲動唯一方式。缺點：設備放置不操作時使用者無法得知有更多股票。

### D4："Next:" 標籤位置
螢幕 240px，`s_card_list` 底邊在 y=206。
"Updated:" 標籤置於 y=207，"Next:" 標籤置於 y=220，底部 33px footer 空間，與捲動容器不重疊。

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| `xTimerGetExpiryTime` 在 timer 停止時回傳值未定義 | 呼叫前先確認 `s_quote_timer != NULL`；scheduler_stop 後 UI 不再收到報價，next label 不會被更新 |
| set_card_count 在 LVGL task 以外呼叫 | 透過 ui_manager 包裝，確保持有 g_ui_mutex 後才操作 lv_obj |
| 股票數為 0（首次開機未設定）| set_card_count(0) 時全部 hidden，顯示 s_empty_label 提示 |
| 捲動容器 bg 透明 | 需設 `bg_opa=LV_OPA_TRANSP` 避免覆蓋 screen 背景色 |
| card 寬度 | 容器寬 = LCD_WIDTH-8，card 寬改為容器寬（`LCD_WIDTH - 8`），與原邏輯一致 |
| 翻頁 timer 與 LVGL task | `lv_timer_create` 在 LVGL task 內執行，天然持有 LVGL context，無需額外持鎖 |
| 手動捲動後被 timer 覆蓋 | 使用者手動滑動後，下次 timer 觸發會跳回自動分頁位置，屬預期行為（看板模式） |
| 只有 1 頁時 timer 仍在跑 | scroll_to_y(0, ...) 呼叫無副作用，不影響效能 |

## Migration Plan

純 UI 與 scheduler API 新增，不涉及 NVS schema 或對外協定變更。直接 OTA / flash 即可，無需資料遷移。
