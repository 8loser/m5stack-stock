## Context

`screen_info.c` 目前是 stub，只有 "Info" 標題與導航提示。現有基礎：

- `ui_manager.h`：`SCREEN_INFO = 3` 已定義
- `ui_manager.c`：`screen_info_create()` 已掛入初始化，`s_nav_screens[]` 已含 `SCREEN_INFO`

可用資料來源：

| Section  | API |
|----------|-----|
| Device   | `esp_get_free_heap_size()`、`esp_chip_info()`、`esp_get_idf_version()` |
| Network  | `storage_wifi_load()`（SSID）、`wifi_manager_get_state()`、`wifi_manager_get_ip()` |
| AI       | `ai_provider_get_name()`、`ai_provider_get_active_api_key()` |
| Stocks   | `storage_stocks_load()`（symbols + count）、`storage_schedule_load()`（排程參數） |

尚未實作：
- `screen_info.c`：4 個 section 的資料讀取與 label 更新
- `screen_info_refresh()`：進入頁面時一次性刷新所有資料
- `ui_manager.c`：switch_screen 至 SCREEN_INFO 時呼叫 refresh
- `CMakeLists.txt`：補 `esp_system`、`esp_wifi` 依賴

## Goals / Non-Goals

**Goals:**
- 顯示 Device / Network / AI / Stocks 四個 section 的唯讀資訊
- 進入頁面時一次性刷新，不做週期性更新
- 可滾動佈局（總內容高度超過 190px 可用空間）
- API key 末 4 碼顯示，其餘遮蔽；未設定時顯示 "Not configured"

**Non-Goals:**
- 即時更新（非 live view）
- 可編輯欄位（唯讀）
- 分頁或折疊 section

## Decisions

### D1：可滾動內容容器

螢幕可用高度：240 - 20（status bar）= 220px。四個 section 估計總高度：

| Section | 高度估算 |
|---------|---------|
| Device（3 行）| header 16 + 3×14 = 58px |
| Network（3 行）| header 16 + 3×14 = 58px |
| AI（2 行）| header 16 + 2×14 = 44px |
| Stocks（最多 12 行）| header 16 + 10×14 + 2×14 = 156px |

總計 > 316px，超過可用空間，**需要捲動**。

實作方式：在 screen 內建立一個可捲動的 `lv_obj_t *container`，設定：
```c
lv_obj_set_size(container, LCD_WIDTH, 190);
lv_obj_set_pos(container, 0, 30);
lv_obj_set_style_pad_all(container, 4, 0);
lv_obj_set_scroll_dir(container, LV_DIR_VER);
// LV_OBJ_FLAG_SCROLLABLE 預設開啟
```

替代方案：讓 screen 本身捲動——但 status_bar 建在 `lv_layer_top()` 不受影響，技術上可行；選用獨立 container 更明確且易調整邊界。

### D2：靜態 Label 預配置策略

Section header（4 個）+ content label（4 個，每個以 `\n` 合併多行）= 8 個靜態 label。

好處：避免 LVGL heap 碎片化；stocks 最多 10 支 + 排程 2 行，全部塞進一個多行 label。

替代方案：每行一個 label（約 24 個）——精確控制顏色，但配置成本高；此頁面為唯讀且無 per-row 色彩需求，多行 label 即可。

Label 配置：
```
container 內（垂直流排列）：
  [section_lbl_0] "DEVICE"              — 顏色 0x4FC3F7，montserrat_12 bold
  [content_lbl_0] "Heap: ...\nChip: ...\nIDF: ..."  — 白色，montserrat_12
  [section_lbl_1] "NETWORK"
  [content_lbl_1] "SSID: ...\nState: ...\nIP: ..."
  [section_lbl_2] "AI"
  [content_lbl_2] "Provider: ...\nKey: ****XXXX"
  [section_lbl_3] "STOCKS"
  [content_lbl_3] "Quote: Xs  AI: Xmin  Market-only: Y\n2330 2317 ..."
```

### D3：API Key 遮蔽邏輯

```c
// api_key = "sk-abc123XYZ"
// 長度 >= 4：顯示 "****" + last 4 chars
// 長度 < 4 或空字串：顯示 "Not configured"
if (strlen(api_key) >= 4) {
    snprintf(buf, sz, "Key: ****%s", api_key + strlen(api_key) - 4);
} else if (strlen(api_key) > 0) {
    snprintf(buf, sz, "Key: ****");
} else {
    snprintf(buf, sz, "Key: Not configured");
}
```

### D4：screen_info_refresh() 呼叫時機

同 log-page 模式：在 `ui_manager_switch_screen()` 的 mutex 持有段新增：

```c
if (id == SCREEN_INFO) {
    extern void screen_info_refresh(void);
    screen_info_refresh();
}
```

`screen_info_refresh()` 直接呼叫各 storage/wifi/ai API 讀取資料（均為同步 NVS 讀取，耗時 < 1ms），更新 8 個 label 文字。

### D5：CMakeLists 依賴補充

`screen_info.c` 需 include：
- `esp_system.h`（`esp_get_free_heap_size`、`esp_chip_info`、`esp_get_idf_version`）
- `wifi_manager.h`（已在 ui component REQUIRES 中）
- `storage.h`（已在 ui component REQUIRES 中）
- `ai_provider.h`（已在 ui component REQUIRES 中）

確認 `components/ui/CMakeLists.txt` REQUIRES 含 `esp_system`；若無則補上。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| NVS 讀取在 UI mutex 持有期間執行 | NVS 同步讀取 < 1ms，不影響 LVGL 渲染節奏 |
| Stocks 顯示超過 10 支（未來擴充）| content_lbl_3 使用 `\n` 串接，上限由 NVS `stock_list_t.count`（最多 10）決定 |
| `wifi_manager_get_ip()` 返回 NULL | 需 NULL check，顯示 "--" |
| `esp_chip_info()` struct 欄位依 IDF 版本不同 | 只用 `model`、`cores`，不用 features bitfield |

## Migration Plan

1. 重寫 `screen_info.c`：container、8 個靜態 label、`screen_info_refresh()`
2. 在 `ui_manager_switch_screen()` 補 SCREEN_INFO 分支
3. 確認 `components/ui/CMakeLists.txt` 含 `esp_system`
4. 驗證：切換至 Info 頁面，確認四個 section 資料正確顯示且可捲動

無 NVS 遷移，無資料流失風險。回滾：`git revert` 單一 commit。
