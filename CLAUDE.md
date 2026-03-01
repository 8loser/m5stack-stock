# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

| 指令 | 說明 |
|------|------|
| `./flash.sh` | Container 內 build + flash + monitor |
| `./flash.sh --build-only` | 只 build |
| `./flash.sh --flash-only` | 只燒錄 |
| `./flash.sh --shell` | 進 container shell 除錯 |
| `./flash.sh --erase` | 清除 NVS 後重新燒錄 |
| `./flash.sh --monitor` | 只開 monitor |

Container runtime 自動偵測 docker/podman，USB device 直通（`/dev/ttyACM0`），image: `docker.io/espressif/idf:v5.1.4`。

## Architecture

ESP-IDF (C) 專案，7 個元件，FreeRTOS 多核心任務。

```
main/app_config.h        # 所有硬體 pin、任務優先級、NVS namespace 常數
main/main.c              # 初始化順序 + 全域 queue/mutex 宣告

components/
  board/                 # HAL 層，所有硬體抽象
  storage/               # NVS 讀寫，唯一持久化介面
  wifi_manager/          # STA 模式，事件驅動，自動重連
  twse_client/           # TWSE API → cJSON → stock_quote_t
  ai_provider/           # vtable 模式三 Provider + 遠端設定下載
  scheduler/             # FreeRTOS Timer 驅動，整合 SNTP + DeepSleep
  ui/                    # LVGL 五頁面，單一 ui_mutex 保護
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

`nvs_flash_init` → `board_init` → `ui_manager_init` → `storage_init` → `wifi_manager_init` → `twse_client_init` → `ai_provider_init` → `scheduler_init`

`ai_provider_init` 內部會依序：
1. 從 NVS 讀 `remote_cfg_url` → HTTP GET → 解析 provider + api_key（存 RAM，不回寫 NVS）
2. 從 NVS 讀 `prompt_url` → HTTP GET → 解析自訂 prompt 模板與額外股票清單

## Gotchas

| 坑點 | 說明 |
|------|------|
| LVGL 非 thread-safe | 所有 `lv_*` 呼叫必須持有 `g_ui_mutex` |
| AI Key 優先順序 | 遠端 Token URL（RAM session）> 本地 NVS `api_key` |
| TWSE 休市回傳 `"-"` | `parse_stock_item` 設 `is_market_closed=true`，顯示昨收 |
| IDF v5.1+ I2S | 用 `i2s_std.h` 新 API；`driver/i2s.h` 已棄用 |
| PSRAM 大型 buffer | `heap_caps_malloc(n, MALLOC_CAP_SPIRAM)`，LVGL/HTTP buffer 優先放 PSRAM |
| GitHub Gist redirect | HTTP client 設 `follow_redirects=true, max_redirection_count=3` |
| BM8563 alarm 暫存器 | `0x80` bit = 不比較；設 alarm 時日期/星期欄位需設 `0x80` |
| LCD flush callback | `lv_disp_flush_ready` 必須在 SPI DMA 傳輸完成後呼叫 |
| Monitor 鎖 port | `--monitor` 持有 `/dev/ttyACM0`；flash 前需先 `kill $(lsof -t /dev/ttyACM0)` |
| UI 字型 | 全英文介面，`UI_FONT_TEXT_DEFAULT = lv_font_montserrat_14`（`components/ui/include/ui_compat.h`）；勿嘗試 CJK 字型 |

## NVS 命名空間

| Namespace | Key | 型別 | 說明 |
|-----------|-----|------|------|
| `wifi_cfg` | `ssid` / `password` | str | WiFi 帳密 |
| `ai_cfg` | `provider` | u8 | 0=Gemini 1=Claude 2=OpenAI |
| `ai_cfg` | `api_key` | str | 本地備用 API Key |
| `ai_cfg` | `remote_cfg_url` | str | 開機自動抓取 Token 的 URL |
| `ai_cfg` | `prompt_url` | str | 遠端 Prompt JSON URL |
| `stocks` | `symbols` / `count` | blob/u8 | 監控股票清單 |
| `schedule` | `quote_ivl` / `ai_ivl` / `mkt_only` | u16/u16/u8 | 排程設定 |

## 遠端設定 JSON 格式

**Token Config**（`remote_cfg_url` 指向）：
```json
{ "provider": "claude", "api_key": "sk-ant-..." }
```

**Prompt Config**（`prompt_url` 指向）：
```json
{
  "system_prompt": "附加給 AI 的系統指示",
  "extra_stocks": ["2330", "2317"],
  "signal_sources": ["twse"],
  "analysis_template": "覆蓋內建 prompt 模板（選填）"
}
```

建議使用 GitHub Gist Raw URL（`gist.githubusercontent.com/...`）存放兩個設定檔。
