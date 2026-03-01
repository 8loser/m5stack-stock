# 計畫：新增 Settings 頁面（調整報價輪詢間隔）

## Context

使用者希望能在裝置上直接調整查詢股價的輪詢時間（`quote_interval_s`），以及市場時段限制（`market_only`）。
現有 `scheduler_apply_config()` 和 `storage_schedule_save/load()` 已支援動態設定更新，缺少的是 UI 入口。
目前只有 SCREEN_DASHBOARD (0) 和 SCREEN_PORTAL (1) 兩個頁面，需新增第 3 個 SCREEN_SETTINGS (2)。

---

## 異動檔案清單

| 動作 | 檔案 |
|------|------|
| 新增 | `components/ui/screens/screen_settings.c` |
| 修改 | `components/ui/include/ui_manager.h` |
| 修改 | `components/ui/ui_manager.c` |
| 修改 | `components/ui/CMakeLists.txt` |

---

## Step 1：新增 `screen_settings.c`

**路徑**：`components/ui/screens/screen_settings.c`

**功能**：
- 從 `scheduler_get_config()` 載入目前設定
- 用 `lv_roller` 選擇 quote_interval_s（選項：30s / 1min / 2min / 5min / 10min）
- 用 `lv_checkbox` 切換 market_only
- BtnB（中鍵）Save：呼叫 `scheduler_apply_config()` 後回到 Dashboard
- BtnA（左鍵）Back：直接回到 Dashboard 不存檔

**頁面佈局（320×220，status bar 佔頂部 20px）**：
```
y=28    SETTINGS  (montserrat_16, 置中)
y=55    "Quote Interval"  (label, 灰色)
y=75    [lv_roller: 30 sec / 1 min / 2 min / 5 min / 10 min]  (寬120, 3行可見)
y=170   [v] Market Hours Only  (lv_checkbox)
y=225   "A:Back   B:Save"  (hint label, 深灰, 底部)
```

**關鍵邏輯**：
```c
static const uint16_t k_intervals[] = {30, 60, 120, 300, 600};

// 載入時找最近的 index
for (int i = 0; i < 5; i++) {
    if (cfg.quote_interval_s <= k_intervals[i]) {
        lv_roller_set_selected(s_roller, i, LV_ANIM_OFF);
        break;
    }
}

// 儲存時
uint16_t idx = lv_roller_get_selected(s_roller);
cfg.quote_interval_s = k_intervals[idx];
cfg.market_only = (lv_obj_get_state(s_cb_market) & LV_STATE_CHECKED);
scheduler_apply_config(&cfg);  // 內部已呼叫 storage_schedule_save()
```

**公開函式**：
```c
lv_obj_t *screen_settings_create(void);   // 由 ui_manager 呼叫
void      screen_settings_on_btn(uint8_t btn);  // btn 0=Back, 1=Save
```

---

## Step 2：修改 `ui_manager.h`

**路徑**：`components/ui/include/ui_manager.h`

新增 `SCREEN_SETTINGS` 到 enum：
```c
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_PORTAL    = 1,
    SCREEN_SETTINGS  = 2,
} screen_id_t;
```

---

## Step 3：修改 `ui_manager.c`

**路徑**：`components/ui/ui_manager.c`

變更點：

1. `s_screens[2]` → `s_screens[3]`（第 78 行）

2. 新增 extern 宣告（第 81~82 行附近）：
   ```c
   extern lv_obj_t *screen_settings_create(void);
   ```

3. 在 `ui_manager_init()` 頁面建立區塊（第 148~150 行附近）：
   ```c
   s_screens[SCREEN_SETTINGS] = screen_settings_create();
   ```

4. `ui_manager_switch_screen()` 邊界檢查（第 178 行）：
   ```c
   if (id >= 3) return;   // 原本是 >= 2
   ```

5. `handle_hw_button()` 新增 Settings 頁面導航規則（第 202~218 行）：
   ```c
   case SCREEN_DASHBOARD:
       if (btn == 0) ui_manager_switch_screen(SCREEN_SETTINGS);   // 新增：左鍵進設定
       if (btn == 1) screen_dashboard_on_btn(1);
       if (btn == 2) ui_manager_switch_screen(SCREEN_PORTAL);
       break;
   case SCREEN_SETTINGS:
   {
       extern void screen_settings_on_btn(uint8_t b);
       screen_settings_on_btn(btn);
       break;
   }
   case SCREEN_PORTAL:
       if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
       break;
   ```

---

## Step 4：修改 `CMakeLists.txt`

**路徑**：`components/ui/CMakeLists.txt`

在 SRCS 清單中新增：
```cmake
"screens/screen_settings.c"
```

---

## 依賴關係

`screen_settings.c` 需要 include：
- `lvgl.h`（LVGL widgets）
- `ui_compat.h`（字型 alias）
- `ui_manager.h`（screen_id_t、switch_screen）
- `scheduler.h`（scheduler_get_config、scheduler_apply_config）
- `storage.h`（schedule_config_t 定義）

這些依賴都已在 `CMakeLists.txt` 的 REQUIRES 中存在（scheduler、storage 均已列出）。

---

## 導航流程

```
Dashboard
  BtnA (左)  → Settings
  BtnB (中)  → 手動刷新報價（不變）
  BtnC (右)  → Portal（不變）

Settings
  BtnA (左)  → 回 Dashboard（不存檔）
  BtnB (中)  → 儲存設定 + 回 Dashboard

Portal
  BtnA (左)  → 回 Dashboard（不變）
```

---

## 驗證方式

1. `./flash.sh --build-only` 確認編譯無誤
2. `./flash.sh` 燒錄後觀察 monitor：
   - 按底部左鍵（BtnA）確認從 Dashboard 切換到 Settings
   - 用 roller 選 "2 min"，按中鍵（BtnB）存檔
   - Monitor 應出現 log：`scheduler: 排程設定已更新：報價=120s AI=...`
   - 回到 Dashboard 後等待，確認 2 分鐘後觸發一次報價抓取
   - 重開機後進 Settings，確認 roller 仍顯示 "2 min"（NVS 持久化）
