---
name: m5stack-core2-dev
description: M5Stack Core2 韌體開發協作，聚焦 ESP-IDF v5.1.x、FreeRTOS、LVGL UI、驅動整合、網路與資料流程除錯與效能調校。當需求涉及 Core2 架構設計、模組修改、UI 事件與執行緒安全、感測與周邊行為、TWSE parser 相容性、heap/stack 調整時使用。不處理燒錄、序列埠監看、erase 或 flash 流程。
---

Routing: m5stack-core2-dev

## Overview

聚焦 M5Stack Core2 的韌體開發與除錯，提供可直接落地的程式修改與驗證策略。
燒錄、flash、monitor 操作一律切換到 `m5stack-core2-flash-agent` 執行，主流程只保留必要摘要與後續修正。

## Workflow

1. 先讀 `docs/hardware_quick_ref.md`，建立最小硬體上下文。
2. 只有在需要 pinmap、官方規格或完整細節時才讀 `docs/hardware_core2_reference.md`。
3. 涉及 TWSE API 欄位語意或 parser 相容性時，優先讀 `docs/twse_api_fields.md`。
4. 變更邏輯時，先定位責任元件再改：`components/board`（硬體）、`components/ui`（畫面）、`components/twse_client`（行情）、`components/scheduler`（排程）、`components/storage`（設定）。
5. 先用最小改動解決問題，避免跨模組重構與行為變更混在同次提交。
6. 需要執行期 log 時，切換 `m5stack-core2-flash-agent` 執行 `./flash.sh --monitor`，取得摘要後回本技能續修。
7. 效能問題先按責任元件拆解（UI/scheduler/network/storage），每輪僅做低風險調整並回歸功能。

## 程式風格與命名慣例

- 語言：C（ESP-IDF + FreeRTOS + LVGL）。
- 縮排：4 個空白；函式大括號另起一行。
- 函式與變數使用 `snake_case`（例：`ui_manager_update_quote`）。
- 巨集與常數使用 `UPPER_SNAKE_CASE`（例：`MAX_STOCK_COUNT`）。
- 模組 API 以元件前綴命名（如 `wifi_manager_*`、`ai_provider_*`）。
- 硬體操作集中在 `components/board/`，上層避免直接呼叫裝置驅動。

## Core2 開發守則

- 所有 `lv_*` 呼叫必須持有 `g_ui_mutex`，避免 LVGL 執行緒競態。
- ESP-IDF v5.1+ 的 I2S 使用 `i2s_std.h` 新 API，不用 `driver/i2s.h` 舊介面。
- 大型 buffer 優先放 PSRAM：`heap_caps_malloc(n, MALLOC_CAP_SPIRAM)`。
- HTTPD handler 堆疊紅線：task stack 4KB，框架開銷後可用 ~2KB。含 `prompt[513]` 的 struct（`stock_alert_config_t` 522B、`at_time_entry_t` 516B）單個就吃半個 stack，陣列直接爆。handler 內 >128B 的 local struct/array 一律 `calloc` 到 heap，所有 error path 需對應 `free()`。
- BM8563 alarm 設定時，日期/星期欄位 `0x80` 代表不比較。
- LCD flush callback 必須在 SPI DMA 傳輸完成後才呼叫 `lv_disp_flush_ready`。
- FT6336U 底部虛擬鍵區域要在輸入層攔截，不傳給 LVGL 一般觸控流程。
- UI 顏色調整先以 `components/ui/screens/screen_dashboard.c` 的校正值為基準，再上板驗證。

## UI/WiFi Gotchas

- Core2 面板顯色有偏移，`lv_color_hex` 名義色不一定等於肉眼顏色；UI 狀態色請先用實機校正值。
- 調整 `lv_obj_set_style_text_color` 時，優先使用 `LV_PART_MAIN | LV_STATE_DEFAULT`，避免 `LV_STATE_ANY` 在部分情境下被樣式鏈覆蓋。
- `screen_log` 是事件 ring buffer，不是即時狀態來源；`Connected` 文字可能是歷史事件，當下連線狀態需看 state API。
- 任何 UI 文案變更後，需執行 `tools/fonts/generate_fonts.sh` 更新字型子集，否則可能出現缺字或亂碼。
- `SCREEN_INFO` 目前語意為「Resource/資源監控頁」；若改頁面名稱，需同步更新 status bar 文案與 README。
- WiFi driver 的 internal DRAM buffer pool 與通用 heap（含 PSRAM）是獨立的記憶體池。Resource screen 顯示 heap 充裕不代表 WiFi driver 有足夠 buffer。STA 有 active traffic 時切 `WIFI_MODE_APSTA`，會因 WiFi internal buffer 不足導致 `ieee80211_hostap_attach` NULL pointer crash。Portal 啟動前的 drain 等待是必要的（釋放 WiFi internal buffer），不可 fire-and-forget。

## 效能調校守則

- FreeRTOS 任務避免 busy loop，優先使用 event-driven，同步補上 `vTaskDelay` 或事件阻塞點。
- stack 調整以任務高水位為依據，避免長期過大浪費或過小造成 overflow/reset。
- 大型與長生命週期 buffer 優先 PSRAM，降低 heap 壓力並減少高頻配置/釋放抖動。
- heap 調校需觀察可用量與碎片趨勢，避免只看單次峰值判斷。
- LVGL 更新保持持鎖區最小化，合併刷新避免過度 redraw。
- 網路與 TWSE 拉取避免過高輪詢，timeout/retry 需可控且可觀測。

## 實作與驗證清單

1. 確認需求影響範圍與模組邊界，列出行為變更點。
2. 修改後執行 `./flash.sh --build-only` 確認可建置（必要時交由 `m5stack-core2-flash-agent`）。
3. 若含效能調校，提供 baseline 與優化後對照（至少涵蓋啟動、畫面更新、單次報價抓取）。
4. 實機驗證主要流程（UI、WiFi、報價抓取、AI、排程）未回歸。
5. heap/stack 調整需附任務高水位與記憶體觀測結果，避免只憑體感調整。
6. 涉及 UI/WiFi/報價/AI/排程的改動，補上可重現手動測試步驟並優先處理 warning/error。
7. 若有 UI 文字異動，驗證步驟需包含：`tools/fonts/generate_fonts.sh` + build-only。

## 跨技能協作策略

### 燒錄與 log 噪訊隔離

燒錄與 monitor 類工作交由 `m5stack-core2-flash-agent` 執行，本技能只保留錯誤摘要、結果與後續修正步驟。

### 平行探索（多元件變更）

涉及 2 個以上元件時，優先使用平行命令讀碼（例如 `rg` + 多檔案並行檢視）收斂影響範圍，避免一次載入過量原始碼。

| 場景 | 做法 |
|------|------|
| 改動涉及 2+ 元件 | 先平行掃描各元件，再整理：影響函式/結構、相依 API、注意事項 |
| 單一元件內部改動 | 主流程直接讀，不需拆分 |
| 不確定影響範圍 | 先做一次全域關鍵字掃描，再決定是否拆分 |

### 平行驗證

可平行執行的驗證步驟應同時進行，不要串行等待：

| 可平行 | 需串行 |
|--------|--------|
| `generate_fonts.sh` + `--build-only` | build 成功 → flash → monitor |
| 多個獨立元件的 header 相容性檢查 | 改 A 的結果影響 B 的改法 |

## Handoff

Portal 網頁調整轉交 `m5stack-portal-web-dev`（`components/portal_backend/portal` 內的 HTML/CSS/JS）。
