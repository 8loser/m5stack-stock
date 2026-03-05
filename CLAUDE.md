# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 與 AGENTS.md 同步

- `CLAUDE.md` 與 `AGENTS.md` 需同步維護。
- 凡涉及開發流程、指令、架構、限制與注意事項之變更，應在同一個變更中同時更新兩份文件。
- 若僅有工具專屬差異，請在兩份文件都註明差異原因。

## Commands

- 指令細節由 `m5stack-core2-dev` 與 `m5stack-core2-flash-agent` 維護，`CLAUDE.md` 僅保留路由與共通規範。
- 測試與驗證步驟細節同樣由 `m5stack-core2-dev` 與 `m5stack-core2-flash-agent` 維護。
- 程式風格與命名細節由 `m5stack-core2-dev` 維護。

## 安全與設定提醒

- 不要提交真實 API Key、WiFi 密碼或私人 Token URL。
- 遠端設定範本目前不在此 repo，機密資訊與實際設定請放在私有端點或私有配置來源。

## Architecture

ESP-IDF (C) 專案，含 app_core event bus，FreeRTOS 多核心任務。

```
main/app_config.h        # 所有硬體 pin、任務優先級、NVS namespace 常數
main/main.c              # 初始化順序 + 全域 queue/mutex 宣告
docs/hardware_quick_ref.md  # 低 token 硬體速查（AI 開發預設先讀）
docs/hardware_core2_reference.md  # Core2 官方規格、PinMap 與完整對照（需要細節時再查）
docs/twse_api_fields.md  # TWSE getStockInfo.jsp 回傳欄位對照與本專案解析規則

components/
  app_core/              # 應用層事件匯流排（跨模組解耦）
  board/                 # HAL 層，所有硬體抽象
  network_portal/        # 對外 façade（統一 network API）
  wifi_manager/          # STA 連線管理、狀態機、AP 掃描
  portal_backend/        # SoftAP + Portal HTTP + Stocks 管理 API
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

`nvs_flash_init` → `app_event_bus_init` → `board_init` → `ui_manager_init` → `storage_init` → `network_portal_init` → `twse_client_init` → `scheduler_init`

## Gotchas

- 開發類 gotchas 由 `m5stack-core2-dev` 維護（避免與 `CLAUDE.md` 重複）。
- 燒錄/連線類 gotchas 由 `m5stack-core2-flash-agent` 維護（例如 monitor 鎖 port）。
- `CLAUDE.md` 僅保留分工與路由規則，不再重複列細節表。

## NVS 命名空間

| Namespace | Key | 型別 | 說明 |
|-----------|-----|------|------|
| `wifi_cfg` | `ssid` / `password` | str | WiFi 帳密 |
| `ai_cfg` | `provider` | u8 | 0=Gemini 1=Claude 2=OpenAI |
| `ai_cfg` | `api_key` | str | API Key |
| `stocks` | `symbols` / `count` | blob/u8 | 監控股票清單 |
| `schedule` | `quote_ivl` / `ai_ivl` / `mkt_only` | u16/u16/u8 | 排程設定 |

## AI 協作分工（去重）

- 功能開發、程式修改與邏輯除錯一律使用 `m5stack-core2-dev`。
- `components/portal_backend/portal` 網頁調整優先使用 `m5stack-portal-web-dev`；若需 firmware/API 行為變更再轉交 `m5stack-core2-dev`。
- 連線、燒錄、監看與 log 取得一律使用 `m5stack-core2-flash-agent`。
- `m5stack-core2-flash-agent` 在燒錄阻塞時可最小修改 `flash.sh`（限連線/燒錄路徑），不得延伸到韌體功能邏輯。
- `m5stack-core2-dev` 需要實機 log 時，先切 `m5stack-core2-flash-agent` 取得結果，再回 `m5stack-core2-dev` 續修。
- 具體流程與守則以各自的 skill/agent 文件為準，`CLAUDE.md` 不重複維護其細節。
