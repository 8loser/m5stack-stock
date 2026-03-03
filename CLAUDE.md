# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 與 AGENTS.md 同步

- `CLAUDE.md` 與 `AGENTS.md` 需同步維護。
- 凡涉及開發流程、指令、架構、限制與注意事項之變更，應在同一個變更中同時更新兩份文件。
- 若僅有工具專屬差異，請在兩份文件都註明差異原因。

## Commands

| 指令 | 說明 |
|------|------|
| `./flash.sh` | Container 內 build + flash + monitor |
| `./flash.sh --build-only` | 只 build |
| `./flash.sh --flash-only [/dev/ttyACM0]` | 只燒錄（需已有 build；可省略序列埠自動偵測） |
| `./flash.sh --app-flash [/dev/ttyACM0]` | 只燒錄 app 分區（開發迭代較快） |
| `./flash.sh --shell` | 進 container shell 除錯 |
| `./flash.sh --erase [/dev/ttyACM0]` | 清除整顆 flash 後重新燒錄（可省略序列埠自動偵測） |
| `./flash.sh --monitor [/dev/ttyACM0]` | 只開 monitor（可省略序列埠自動偵測） |

Container runtime 自動偵測 docker/podman，USB device 直通（`/dev/ttyACM0`），image: `docker.io/espressif/idf:v5.1.4`。

## Architecture

ESP-IDF (C) 專案，含 app_core event bus，FreeRTOS 多核心任務。

```
main/app_config.h        # 所有硬體 pin、任務優先級、NVS namespace 常數
main/main.c              # 初始化順序 + 全域 queue/mutex 宣告
docs/hardware_quick_ref.md  # 低 token 硬體速查（AI 開發預設先讀）
docs/hardware_core2_reference.md  # Core2 官方規格、PinMap 與完整對照（需要細節時再查）

components/
  app_core/              # 應用層事件匯流排（跨模組解耦）
  board/                 # HAL 層，所有硬體抽象
  device_server/         # WiFi + Portal + Stocks 管理（已拆分 service）
  storage/               # NVS 讀寫，唯一持久化介面
  twse_client/           # TWSE API → cJSON → stock_quote_t
  ai_provider/           # vtable 模式三 Provider（Gemini/Claude/OpenAI）
  scheduler/             # FreeRTOS Timer 驅動，整合 SNTP + DeepSleep（透過 event bus 回報 heartbeat）
  ui/                    # LVGL 頁面與 widgets，單一 ui_mutex 保護
```

## 關鍵資料流

```
scheduler Timer
  → twse_client_fetch()
  → xQueueSend(g_quote_queue)
  → ui_manager_update_quote()      # Dashboard 顯示

scheduler Timer（ai_ivl）
  → ai_provider_analyze_async()    # 建立獨立 FreeRTOS task
  → xQueueOverwrite(g_ai_result_queue)
  → ui_manager_update_ai_result()  # AI 分析頁面顯示
```

## 開機初始化順序

`nvs_flash_init` → `app_event_bus_init` → `board_init` → `ui_manager_init` → `storage_init` → `device_server_init` → `twse_client_init` → `scheduler_init`

## Gotchas

| 坑點 | 說明 |
|------|------|
| LVGL 非 thread-safe | 所有 `lv_*` 呼叫必須持有 `g_ui_mutex` |
| TWSE 休市回傳 `"-"` | `parse_stock_item` 設 `is_market_closed=true`，顯示昨收 |
| IDF v5.1+ I2S | 用 `i2s_std.h` 新 API；`driver/i2s.h` 已棄用 |
| PSRAM 大型 buffer | `heap_caps_malloc(n, MALLOC_CAP_SPIRAM)`，LVGL/HTTP buffer 優先放 PSRAM |
| BM8563 alarm 暫存器 | `0x80` bit = 不比較；設 alarm 時日期/星期欄位需設 `0x80` |
| LCD flush callback | `lv_disp_flush_ready` 必須在 SPI DMA 傳輸完成後呼叫 |
| Monitor 鎖 port | `--monitor` 持有 `/dev/ttyACM0`；flash 前需先 `kill $(lsof -t /dev/ttyACM0)` |
| UI 字型 | `UI_FONT_TEXT_DEFAULT = lv_font_noto_tc_14`（`components/ui/include/ui_compat.h`）；時間/WiFi icon 用 `lv_font_montserrat_14` |
| Core2 色彩校正 | 此面板在目前驅動下標準 RGB hex 可能偏色；UI 新增/調整顏色請先用 `components/ui/screens/screen_dashboard.c` 的 `COLOR_UP/DOWN/FLAT` 實機校正值做基準，再上板確認 |
| LVGL event callback 重用 | 不可傳 dummy `lv_event_t{}`（code=0 = `LV_EVENT_ALL`，CLICKED check 失敗）；改抽 helper function 直接呼叫 |
| FT6336U 底部虛擬按鍵 | FT6336U 韌體固定回報值；實測 y=270–279（x: A≈95, B≈190, C≈272–290）；`TOUCH_BTN_Y_MIN=LCD_HEIGHT`（240）攔截，不傳給 LVGL |
| Portal 前端維護位置 | 改 `components/device_server/portal/index.html`；C 端透過 `EMBED_TXTFILES` 內嵌，不再手寫長 HTML 字串 |

## NVS 命名空間

| Namespace | Key | 型別 | 說明 |
|-----------|-----|------|------|
| `wifi_cfg` | `ssid` / `password` | str | WiFi 帳密 |
| `ai_cfg` | `provider` | u8 | 0=Gemini 1=Claude 2=OpenAI |
| `ai_cfg` | `api_key` | str | API Key |
| `stocks` | `symbols` / `count` | blob/u8 | 監控股票清單 |
| `schedule` | `quote_ivl` / `ai_ivl` / `mkt_only` | u16/u16/u8 | 排程設定 |

## AI 開發上下文最小化

- 本專案以 AI 協作為主，預設先讀 `docs/hardware_quick_ref.md`，避免每次載入完整硬體文件。
- 只有在需要 pinmap 背景、官方連結或完整規格時，才展開 `docs/hardware_core2_reference.md`。
