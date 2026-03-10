# 倉庫貢獻指南

## 專案結構與模組規劃
本專案是 M5Stack Core2 的 ESP-IDF（`v5.1.x`）韌體專案。

- `main/`：程式入口與共用設定（`main.c`、`app_config.h`）
- `components/`：功能模組（board HAL、app_core event bus、network_portal façade、wifi_manager、portal_backend、storage、TWSE client、AI provider、scheduler_service、UI）
- `docs/hardware_quick_ref.md`：低 token 硬體速查（AI 開發預設先讀）
- `docs/hardware_core2_reference.md`：Core2 官方規格、PinMap 與完整對照（需要細節時再查）
- `docs/twse_api_fields.md`：TWSE `getStockInfo.jsp` 回傳欄位對照與本專案解析規則
- `build/`：建置產物（請勿手動修改）
- 根目錄設定：`CMakeLists.txt`、`partitions.csv`、`sdkconfig*`、`idf_component.yml`

## 建置、測試與開發指令
- 指令細節由 `m5stack-core2-dev` 與 `m5stack-core2-flash-agent` 維護，`AGENTS.md` 僅保留路由與共通規範。

## 程式風格與命名慣例
- 程式風格與命名細節由 `m5stack-core2-dev` 維護，`AGENTS.md` 僅保留路由與共通規範。

## 測試準則
- 測試與驗證步驟細節由 `m5stack-core2-dev` 與 `m5stack-core2-flash-agent` 維護，`AGENTS.md` 僅保留路由與共通規範。
- 若變更邏輯較大，PR 仍需附上可重現的手動測試清單。

## Firmware 併發與資源基線
- 韌體功能預設採非阻塞設計：耗時流程應放在獨立 task，模組間以 queue/event bus 解耦；避免在 UI 主迴圈或高頻控制路徑執行長時間同步網路呼叫。
- 週期性工作需控制輪詢頻率，避免過高 polling；timeout/retry/backoff 必須可設定或可觀測。
- 外部訊息與 JSON 處理需設定大小上限（request/response buffer、解析長度、欄位長度），避免無界成長造成記憶體壓力。
- AtTime 觸發 AI 前，若 Telegram polling 正在運行，需先 pause 並等待 in-flight HTTP idle，再進行 AI 呼叫，完成後再 resume，避免共享網路資源互相干擾。
- 需保留基本記憶體可觀測性：至少監控 heap 可用量趨勢與任務 stack high-water mark，並在效能調校/回歸時附前後對照。

### Internal DRAM 限制（Core2 硬體紅線）

| 限制 | 說明 |
|------|------|
| Task stack 預設用 internal DRAM | 新增 task 前必須確認剩餘 internal DRAM 夠用 |
| PSRAM 不可作 task stack | NVS/SPI flash 操作時 cache 關閉，PSRAM 不可存取（assert crash） |
| APSTA（配網實驗功能） | 僅在「未連 WiFi 的 Portal 配網模式」啟用；可讓手機連 Core2 AP 後直接掃描周邊 AP。Core2 仍有 `ieee80211_hostap_attach` 風險，需監控 internal DRAM 趨勢與 scan 失敗率 |
| HTTPD handler 禁止 outbound HTTPS fetch | handler 內呼叫 `twse_client_fetch` 會與 portal inbound sockets 搶 lwIP socket slot → crash；僅允許單次驗證呼叫（`twse_client_validate_symbol`），報價由 scheduler resume 後補抓 |
| HTTPD handler 堆疊紅線 | stack 4KB，框架開銷後可用 ~2KB；含 `prompt[513]` 的 struct（`stock_alert_config_t` 522B、`at_time_entry_t` 516B）單個就吃半個 stack，陣列直接爆；>128B 的 local struct/array 一律 `calloc` 到 heap |
| `esp_http_client` 非 thread-safe | 不可從其他 thread 呼叫 `esp_http_client_close/cleanup`；stop 只設 flag，讓 owner task 自行清理 |
| TCP TIME_WAIT 佔 socket slot | `CONFIG_LWIP_TCP_MSL=3000`（6s TIME_WAIT）；portal server 用 SO_LINGER 送 RST；**必須確認 `CONFIG_LWIP_SO_LINGER=y`**（未啟用時 `setsockopt` 靜默無效） |

### Portal 雙模式架構

| WiFi 狀態 | Portal 模式 | Bot task | 說明 |
|-----------|------------|----------|------|
| 已連線 | STA（不切 WiFi） | pause + cmd queue | 非阻塞，外網可用 |
| 未連線 | APSTA（實驗） | stop（釋放 DRAM） | 配網用；手機連 Core2 AP 時可執行 WiFi scan |

### Portal 期間網路資源協調

- HTTPD handler 內禁止 outbound HTTPS quote fetch（twse_client_fetch）；僅允許 `twse_client_validate_symbol`（單次驗證）
- `scheduler_service_trigger_quote_now()` 需檢查 `quote_polling_paused`，portal 期間不觸發
- scheduler task 的 force/normal fetch 都需 `quote_polling_paused` guard
- `scheduler_service_resume_quote_polling()` 立即 force fetch（portal 關閉 → dashboard 即時有資料）
- 參考模式：telegram bot 的 pause → wait_http_idle → resume 三步驟

## Portal 前端驗收基線
- Portal UI 必須支援手機與桌面瀏覽（`360px` / `768px` / `1280px`）。
- 在上述寬度下不得出現水平捲動、主要文案不可讀或主要按鈕不可操作。
- 主要流程需可用觸控完成（手機）。
- 考量 Core2 資源有限，前端實作需優先採用輕量化方案（低記憶體、低傳輸量、低靜態資源體積）。
- 避免冗餘寫法與無實際作用程式碼（dead code、重複邏輯、無效監聽與未使用資源）。
- 當使用者對 UI 的主訴包含「太大」、「太擠」、「太醜」時，需先拆成尺寸、間距、風格三類問題處理，不得用單一大幅重設計混合解決。
- 若使用者明確指出元件「太大」，後續方案必須先縮小尺寸、降低首屏佔用，再處理視覺風格；不得以增加留白、放大元件或加重容器存在感作為主要解法。
- 導覽元件預設應弱於主內容；除非使用者明確要求，導覽不得設計成主視覺卡片或功能磚牆。

## Commit 與 Pull Request 規範
- Commit 建議使用前綴式訊息；目前歷史有 `init:`，建議採用 `feat:`、`fix:`、`refactor:`、`docs:`、`chore:`。
- 每個 Commit 聚焦單一目的，避免把重構與行為變更混在一起。
- PR 需說明變更內容與原因。
- PR 需列出已執行的軟硬體驗證步驟。
- 有對應議題時請附上 issue 連結。
- 涉及 UI 變更請附截圖或錄影。
- Portal UI 變更需附手機與桌面前後對照截圖，以及可重現的手動測試步驟。

## 安全與設定提醒
- 不要提交真實 API Key、WiFi 密碼或私人 Token URL。
- 遠端設定範本目前不在此 repo，機密資訊與實際設定請放在私有端點或私有配置來源。

## TODO 收件匣規範
- 使用者提到 TODO、待辦、臨時想法且「尚未進入規劃」時，預設寫入 `docs/todo_inbox.md`。
- 寫入格式以單行待辦為主：`- [ ] YYYY-MM-DD: TODO ...`。
- 進入正式規劃或實作前，再將項目搬移至 `openspec/changes/.../tasks.md`。

## Architecture
ESP-IDF (C) 專案，含 app_core event bus，FreeRTOS 多核心任務。
- 目錄與模組清單請見「專案結構與模組規劃」，本節不重複列舉。
- 模組邊界以 event bus/queue 解耦，避免跨模組直接耦合。
- UI 操作需維持單一 `ui_mutex` 保護的一致性。

## 關鍵資料流
```
scheduler_service Timer
  → twse_client_fetch()
  → xQueueSend(g_quote_queue)
  → ui_manager_update_quote()      # Dashboard 顯示
```

## 開機初始化順序
`nvs_flash_init` → `app_event_bus_init` → `board_init` → `ui_manager_init` → `storage_init` → `network_portal_init` → `twse_client_init` → `scheduler_service_init`

## NVS 命名空間

| Namespace | Key | 型別 | 說明 |
|-----------|-----|------|------|
| `wifi_cfg` | `ssid` / `password` | str | WiFi 帳密 |
| `ai_cfg` | `provider` | u8 | 0=Gemini 1=Claude 2=OpenAI |
| `ai_cfg` | `api_key` | str | API Key |
| `stocks` | `symbols` / `count` | blob/u8 | 監控股票清單 |
| `schedule` | `quote_ivl` / `ai_ivl` / `mkt_only` | u16/u16/u8 | 排程設定 |

## AI 文件同步規範
- `AGENTS.md` 與 `CLAUDE.md` 需同步維護；凡共通規範變更，兩份文件必須同次更新。
- 若僅更新其中一份，必須在該文件標註「不同步原因」與適用範圍（工具專屬差異）。

## Gotchas
- 開發類 gotchas 由 `m5stack-core2-dev` 維護（避免與 `AGENTS.md` 重複）。
- 燒錄/連線類 gotchas 由 `m5stack-core2-flash-agent` 維護（例如 monitor 鎖 port）。
- `AGENTS.md` 僅保留分工與路由規則，不再重複列細節表。
- 連線判讀規則：`WiFi connected (STA)` 不等於 `Provisioning Portal active (AP/HTTP)`；診斷 Portal 頁面時不得只憑 STA 已連線判定正常。
- Portal 現為 STA/APSTA 雙模式：已連 WiFi 時不切 WiFi（STA 直接服務）；未連 WiFi 時啟用 APSTA（實驗）以支援手機連 Core2 AP 後掃描周邊 WiFi，並持續監控 internal DRAM/scan 失敗率。
- sdkconfig 未啟用的功能會讓對應 C API 靜默失敗（如 SO_LINGER、SO_RCVBUF）；排查 lwIP/網路問題時優先檢查 sdkconfig 選項。
- 修 bug 優先確認根因再動手，避免在根因未確認前建立大型 workaround（如 async state machine + polling endpoint）。

## AI 協作分工（去重）
- 功能開發、程式修改與邏輯除錯一律使用 `m5stack-core2-dev`。
- `components/portal_backend/portal` 網頁調整優先使用 `m5stack-portal-web-dev`；若需 firmware/API 行為變更再轉交 `m5stack-core2-dev`。
- 燒錄、flash、monitor、log 擷取一律交由 `m5stack-core2-flash-agent` 執行，主對話只接收摘要。
- `m5stack-core2-flash-agent` 在燒錄阻塞時可最小修改 `flash.sh`（限連線/燒錄路徑），不得延伸到韌體功能邏輯。
- 具體流程與守則以各自的 skill/agent 文件為準，`AGENTS.md` 不重複維護其細節。

## 路由可觀測性
- 每個任務開始時，第一則進度訊息必須明確標示：`Routing: <skill-name>`。
- 若任務中途改派（例如 `m5stack-core2-dev` 轉 `m5stack-core2-flash-agent`），需再補一則路由切換訊息。
- 計劃中的驗證步驟（build/flash/monitor）也需標記由哪個 skill 執行，避免實作時遺漏路由切換。
- 當 `Routing: m5stack-portal-web-dev` 時，需補一行：`Scope: portal static files only; No firmware/API changes`。

## 重複異常修正升級通知
- 同一任務內，若同類異常修正次數超過兩次（第 3 次起），必須主動通知使用者。
- 通知內容需包含前兩次調整摘要：每次都要交代「改了什麼、結果如何、為何尚未解決」。
- 通知內容需補上本輪方向建議：至少提供兩個可選方向，並標示建議採用方向與理由，供使用者判斷是否調整策略。
- 「同類異常」以同一症狀且同一主要根因群組為判定基準；若已確認為新根因，可重新計數。
