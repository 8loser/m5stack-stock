---
name: m5stack-core2-dev
description: M5Stack Core2 韌體開發協作技能，聚焦 ESP-IDF v5.1.x、FreeRTOS、LVGL UI、驅動整合、網路與資料流程除錯與效能調校。當需求涉及 Core2 架構設計、模組修改、UI 事件與執行緒安全、感測與周邊行為、TWSE parser 相容性、heap/stack 調整時使用。本技能不處理燒錄、序列埠監看、erase 或 flash 流程。
---

# M5Stack Core2 Dev

## Overview

聚焦 M5Stack Core2 的韌體開發與除錯，提供可直接落地的程式修改與驗證策略。
若任務是 `flash.sh` 相關燒錄、監看、清除流程，直接轉交專用 flash skill 或 agent。

## Workflow

1. 先讀 `docs/hardware_quick_ref.md`，建立最小硬體上下文。
2. 只有在需要 pinmap、官方規格或完整細節時才讀 `docs/hardware_core2_reference.md`。
3. 涉及 TWSE API 欄位語意或 parser 相容性時，優先讀 `docs/twse_api_fields.md`。
4. 變更邏輯時，先定位責任元件再改：`components/board`（硬體）、`components/ui`（畫面）、`components/twse`（行情）、`components/scheduler`（排程）、`components/storage`（設定）。
5. 先用最小改動解決問題，避免跨模組重構與行為變更混在同次提交。
6. 進行效能調校時先建立 baseline（啟動時間、畫面更新延遲、單次報價抓取耗時），再做優化與前後對照。
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
- BM8563 alarm 設定時，日期/星期欄位 `0x80` 代表不比較。
- LCD flush callback 必須在 SPI DMA 傳輸完成後才呼叫 `lv_disp_flush_ready`。
- FT6336U 底部虛擬鍵區域要在輸入層攔截，不傳給 LVGL 一般觸控流程。
- UI 顏色調整先以 `components/ui/screens/screen_dashboard.c` 的校正值為基準，再上板驗證。

## UI/WiFi Gotchas

- Core2 面板顯色有偏移，`lv_color_hex` 名義色不一定等於肉眼顏色；UI 狀態色請先用實機校正值。
- 調整 `lv_obj_set_style_text_color` 時，優先使用 `LV_PART_MAIN | LV_STATE_DEFAULT`，避免 `LV_STATE_ANY` 在部分情境下被樣式鏈覆蓋。
- `screen_log` 是事件 ring buffer，不是即時狀態來源；`Connected` 文字可能是歷史事件，當下連線狀態需看 state API。

## 效能調校守則

- FreeRTOS 任務避免 busy loop，優先使用 event-driven，同步補上 `vTaskDelay` 或事件阻塞點。
- stack 調整以任務高水位為依據，避免長期過大浪費或過小造成 overflow/reset。
- 大型與長生命週期 buffer 優先 PSRAM，降低 heap 壓力並減少高頻配置/釋放抖動。
- heap 調校需觀察可用量與碎片趨勢，避免只看單次峰值判斷。
- LVGL 更新保持持鎖區最小化，合併刷新避免過度 redraw。
- 網路與 TWSE 拉取避免過高輪詢，timeout/retry 需可控且可觀測。

## 實作與驗證清單

1. 確認需求影響範圍與模組邊界，列出行為變更點。
2. 修改後至少通過 `./flash.sh --build-only`。
3. 若含效能調校，提供 baseline 與優化後對照（至少涵蓋啟動、畫面更新、單次報價抓取）。
4. 實機驗證主要流程（UI、WiFi、報價抓取、AI、排程）未回歸。
5. heap/stack 調整需附任務高水位與記憶體觀測結果，避免只憑體感調整。
6. 需要執行期 log 時，切換 `m5stack-core2-flash-agent` 使用 `--monitor` 取得 log，再回本技能續修。
7. 涉及 UI/WiFi/報價/AI/排程的改動，補上可重現手動測試步驟並優先處理 warning/error。

## Handoff

若使用者需求包含以下任一項，停止本技能流程並轉交 flash 專用能力：
- `./flash.sh` 的燒錄、`--flash-only`、`--app-flash`
- `--monitor` 序列埠監看與 port 佔用處理
- `--erase` 全清後重刷
