## Context

`screen_settings.c` 目前不存在（無 stub）。現有基礎：

- `ui_manager.h`：`SCREEN_INFO = 3`、`SCREEN_COUNT`，無 `SCREEN_SETTINGS`
- `ui_manager.c`：`s_nav_screens[] = {SCREEN_DASHBOARD, SCREEN_LOG, SCREEN_INFO}`
- `CMakeLists.txt`：REQUIRES 已含 `scheduler`、`storage`；SRCS 無 `screen_settings.c`
- `scheduler.h`：`scheduler_get_config()`、`scheduler_apply_config()` 已提供讀寫排程設定

可調整的排程參數（`schedule_config_t`）：
- `quote_interval_s`（uint16_t）：報價輪詢間隔（秒）
- `market_only`（bool）：是否限制市場時段

## Goals / Non-Goals

**Goals:**
- 提供 `lv_roller` 選擇輪詢間隔（5 個固定選項）
- 提供 `lv_checkbox` 切換 Market Hours Only
- 儲存後透過 `scheduler_apply_config()` 即時生效、`storage_schedule_save()` 持久化
- 進入頁面時從 `scheduler_get_config()` 載入當前值

**Non-Goals:**
- 調整 AI 分析間隔（`ai_interval_min`）——留待 Portal 網頁設定
- 輸入任意數值（只提供固定選項）
- 硬體按鍵 save / discard 快捷鍵（見 D1）

## Decisions

### D1：導航入口——改為 Portal 觸控按鈕（偏離原 proposal）

**原 proposal**：Dashboard `btn=0`（左鍵）進入 Settings。

**衝突**：`button-navigation-remap` change 已實作後，`btn=0/2` 用於在 `s_nav_screens[]` 間輪詢；`button-navigation-remap` design 明確註記「SCREEN_SETTINGS 暫不加入（功能性頁面，非主內容流）」。

**決策**：在 Portal 頁面右側面板新增一個 "Settings" 觸控按鈕（`lv_btn`），按下後呼叫 `ui_manager_switch_screen(SCREEN_SETTINGS)`。Portal 作為設定入口中樞，語義一致。

替代方案：加入 `s_nav_screens[]`——但 remap 設計已明確排除，破壞「左右 = 主內容流」語義。

### D2：Save / Cancel 改為觸控按鈕（不使用硬體按鍵）

**原 proposal**：btn=0 = 不存檔返回，btn=1 = 儲存後返回。

**衝突**：`button-navigation-remap` 已賦予 btn=1 全域語義（進入/離開 Portal）。在 Settings 頁面攔截 btn=1 做 Save 會破壞這個語義。

**決策**：Settings 頁面底部放置兩個觸控按鈕：
- **Save**（綠色）：`scheduler_apply_config()` + `storage_schedule_save()` → `ui_manager_switch_screen(SCREEN_PORTAL)`
- **Cancel**（灰色）：不儲存 → `ui_manager_switch_screen(SCREEN_PORTAL)`

硬體按鍵維持全域語義（btn=1 進 Portal，btn=0/2 輪詢 nav screens）。

### D3：SCREEN_SETTINGS 位置

加入 enum 為 `SCREEN_SETTINGS = 4`（接在 `SCREEN_INFO = 3` 之後），`SCREEN_COUNT` 自動變為 5。

### D4：Roller 選項與數值映射

```c
static const uint16_t INTERVAL_VALUES[] = { 30, 60, 120, 300, 600 };
// roller options string：
"30 sec\n1 min\n2 min\n5 min\n10 min"
```

載入時：遍歷 `INTERVAL_VALUES` 找 `quote_interval_s` 的最接近項，設定 roller 初始選中。未命中則 fallback 到 index 1（60s）。

### D5：頁面佈局（固定，無需捲動）

```
y=22   Title: "Settings"  (montserrat_20, 置中)
y=50   Label: "Quote Interval"  (montserrat_12)
y=68   lv_roller  (寬200, 置中, 顯示3行)
y=148  Label: "Market Hours Only"  (montserrat_12)
y=168  lv_checkbox  (置中)
y=200  [Cancel] [Save]  (各寬120, 底部對齊)
```

總高度 < 220px，不需捲動。

### D6：Save 流程

```c
// 儲存按鈕回調（在 LVGL event handler 中同步執行）
schedule_config_t cfg;
scheduler_get_config(&cfg);  // 取得 ai_interval_min 等不調整的欄位
cfg.quote_interval_s = INTERVAL_VALUES[lv_roller_get_selected(s_roller)];
cfg.market_only      = lv_checkbox_is_checked(s_checkbox);
scheduler_apply_config(&cfg);
storage_schedule_save(&cfg);
ui_manager_switch_screen(SCREEN_PORTAL);
```

注意：`scheduler_apply_config()` 與 `storage_schedule_save()` 在 LVGL event callback 中同步執行。兩者均為 NVS/FreeRTOS timer 操作，預計 < 5ms，畫面短暫凍結可接受（與現有 Portal save 行為一致）。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| Portal 頁面 UI 修改需調整現有佈局 | Settings 按鈕放置於右側面板底部，不影響 QR 區 |
| Save callback 同步執行 NVS 寫入 | 同現有 Portal save 模式，< 5ms，接受此 trade-off |
| `quote_interval_s` 值不在固定選項中 | 載入時 fallback 到最接近值，確保 roller 始終有合法選中項 |
| SCREEN_COUNT 增為 5，s_screens[] 需更新 | `ui_manager.c` 中 s_screens[SCREEN_COUNT] 為靜態陣列，自動擴充 |

## Migration Plan

1. `ui_manager.h`：加入 `SCREEN_SETTINGS = 4`
2. 新增 `components/ui/screens/screen_settings.c`（roller + checkbox + Save/Cancel）
3. `ui_manager.c`：init 加 `screen_settings_create()`；`switch_screen` 加 `screen_settings_load()` 呼叫
4. `screen_portal.c`：右側面板新增 Settings 觸控按鈕
5. `CMakeLists.txt`：SRCS 加 `screen_settings.c`
6. 驗證：Portal → Settings → 調整 → Save/Cancel → 確認行為

無 NVS schema 變更（使用既有 `schedule` namespace）。回滾：`git revert`。
