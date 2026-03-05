---
name: m5stack-core2-dev
description: M5Stack Core2 韌體開發協作技能，聚焦 ESP-IDF v5.1.x、FreeRTOS、LVGL UI、驅動整合、網路與資料流程除錯。當需求涉及 Core2 架構設計、模組修改、UI 事件與執行緒安全、感測與周邊行為、TWSE parser 相容性時使用。本技能不處理燒錄、序列埠監看、erase 或 flash 流程。
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

## 實作與驗證清單

1. 確認需求影響範圍與模組邊界，列出行為變更點。
2. 修改後至少通過 `./flash.sh --build-only`。
3. 實機驗證主要流程（UI、WiFi、報價抓取、AI、排程）。
4. 需要執行期 log 時，切換 `m5stack-core2-flash-agent` 使用 `--monitor` 取得 log，再回本技能續修。
5. 涉及 UI/WiFi/報價/AI/排程的改動，補上可重現手動測試步驟並優先處理 warning/error。

## Handoff

若使用者需求包含以下任一項，停止本技能流程並轉交 flash 專用能力：
- `./flash.sh` 的燒錄、`--flash-only`、`--app-flash`
- `--monitor` 序列埠監看與 port 佔用處理
- `--erase` 全清後重刷
