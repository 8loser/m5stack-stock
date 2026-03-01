# Plan: 新增 Info 頁面

## Context

目前 UI 只有 Dashboard（股票行情）和 Portal（WiFi 配網）兩頁。使用者希望新增 Info 頁面，顯示裝置狀態、網路資訊、AI 設定、股票設定。導航：左鍵在 Dashboard ↔ Info 循環，Portal 保持右鍵進出。

---

## 導航規則（變更後）

| 頁面 | 左鍵（btn=0） | 中鍵（btn=1） | 右鍵（btn=2） |
|------|-------------|-------------|-------------|
| Dashboard | **進入 Info**（原無動作） | 手動刷新股價（不變） | 進入 Portal（不變） |
| Info | **返回 Dashboard** | - | - |
| Portal | 返回 Dashboard（不變） | - | - |

---

## Info 頁面內容（可滾動）

```
[Device]
  Chip:  ESP32-D0WDQ6
  Heap:  123 KB free
  PSRAM: 1234 KB / 4096 KB
  IDF:   v5.1.4

[Network]
  Status: Connected
  SSID:   MyWifi
  IP:     192.168.1.100
  RSSI:   -65 dBm

[AI]
  Provider: Gemini
  Key:      ****abcd       ← 末4碼；若未設定顯示 "Not configured"

[Stocks]
  Symbols:  2330 2317 2454 2412 3008
  Quote:    60s
  AI:       30 min
  Mkt only: Yes
```

---

## 資料來源 API

| Section | 呼叫 |
|---------|------|
| Device | `esp_chip_info()`, `esp_get_free_heap_size()`, `esp_psram_get_size()`, `esp_idf_version_string` |
| Network | `wifi_manager_get_state()`, `wifi_manager_get_ip()`, `storage_wifi_load()`（SSID）, `esp_wifi_sta_get_ap_info()`（RSSI） |
| AI | `storage_ai_load_provider()`, `storage_ai_load_provider_key()`, `ai_provider_get_name()` |
| Stocks | `storage_stocks_load()`, `storage_schedule_load()` |

---

## Task 清單

- [ ] **T1** 修改 `components/ui/include/ui_manager.h`
  - 新增 `SCREEN_INFO = 2` 到 `screen_id_t` enum
  - 前向宣告 `lv_obj_t *screen_info_create(void);`
  - 前向宣告 `void screen_info_refresh(void);`

- [ ] **T2** 新增 `components/ui/screens/screen_info.c`
  - `screen_info_create()` — 建立可滾動佈局、4 個 section、所有 label 初始為空
  - `screen_info_refresh()` — 讀取所有資料並更新 labels（masked key 邏輯）
  - `screen_info_on_btn()` — btn==0 切換到 DASHBOARD

- [ ] **T3** 修改 `components/ui/ui_manager.c`
  - `s_screens[]` 陣列改為 `[3]`
  - `ui_manager_init()` 加 `s_screens[SCREEN_INFO] = screen_info_create();`
  - `ui_manager_switch_screen()` 有效範圍改為 `< 3`；進入 INFO 時呼叫 `screen_info_refresh()`
  - `handle_hw_button()` — Dashboard btn=0 新增切換 INFO；新增 SCREEN_INFO case

- [ ] **T4** 修改 `components/ui/CMakeLists.txt`
  - SRCS 加 `"screens/screen_info.c"`
  - REQUIRES 加 `esp_system`、`esp_wifi`

- [ ] **T5** Build & Flash 驗證

---

## 關鍵實作細節

### screen_info.c static 變數

```c
static lv_obj_t *s_screen;
// Device labels
static lv_obj_t *s_lbl_chip, *s_lbl_heap, *s_lbl_psram, *s_lbl_idf;
// Network labels
static lv_obj_t *s_lbl_net_status, *s_lbl_ssid, *s_lbl_ip, *s_lbl_rssi;
// AI labels
static lv_obj_t *s_lbl_ai_provider, *s_lbl_ai_key;
// Stock labels
static lv_obj_t *s_lbl_symbols, *s_lbl_quote_ivl, *s_lbl_ai_ivl, *s_lbl_mkt;
```

### Masked Key 邏輯

```c
char key[128] = {0};
storage_ai_load_provider_key(provider_type, key, sizeof(key));
size_t klen = strlen(key);
if (klen >= 4)
    snprintf(buf, sizeof(buf), "Key: ****%s", key + klen - 4);
else if (klen > 0)
    snprintf(buf, sizeof(buf), "Key: ****");
else
    snprintf(buf, sizeof(buf), "Key: Not configured");
```

### handle_hw_button 變更

```c
case SCREEN_DASHBOARD:
    if (btn == 0) ui_manager_switch_screen(SCREEN_INFO);   // 新增
    if (btn == 1) screen_dashboard_on_btn(1);
    if (btn == 2) ui_manager_switch_screen(SCREEN_PORTAL);
    break;

case SCREEN_INFO:
    if (btn == 0) ui_manager_switch_screen(SCREEN_DASHBOARD);
    break;
```

### switch_screen 新增 INFO 刷新

```c
if (id == SCREEN_INFO) screen_info_refresh();
```

---

## 修改的檔案清單

| 檔案 | 動作 |
|------|------|
| `components/ui/include/ui_manager.h` | 修改 |
| `components/ui/screens/screen_info.c` | 新增 |
| `components/ui/ui_manager.c` | 修改 |
| `components/ui/CMakeLists.txt` | 修改 |

---

## Verification

1. `./flash.sh --build-only` — 無 error/warning
2. 燒錄後測試導航循環：Dashboard 左鍵 → Info → 左鍵 → Dashboard
3. Info 右鍵/中鍵無反應（符合預期）
4. Portal 進出行為不變（右鍵進、左鍵出）
5. WiFi 未連線時 Network section 顯示 "Disconnected"，IP/RSSI 顯示 "N/A"
6. AI key 未設定時顯示 "Not configured"
