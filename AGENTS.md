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
- 需保留基本記憶體可觀測性：至少監控 heap 可用量趨勢與任務 stack high-water mark，並在效能調校/回歸時附前後對照。

### Internal DRAM 限制（Core2 硬體紅線）

| 限制 | 說明 |
|------|------|
| Task stack 預設用 internal DRAM | 新增 task 前必須確認剩餘 internal DRAM 夠用 |
| PSRAM 不可作 task stack | NVS/SPI flash 操作時 cache 關閉，PSRAM 不可存取（assert crash） |
| APSTA 模式不可用 | WiFi AP+STA 同時運行需要大量 internal DRAM，Core2 上會 crash（`ieee80211_hostap_attach`） |
| HTTPD handler 大 buffer 需移 heap | HTTPD task stack 僅 4KB，>128B 的 local array 應 malloc |
| `esp_http_client` 非 thread-safe | 不可從其他 thread 呼叫 `esp_http_client_close/cleanup`；stop 只設 flag，讓 owner task 自行清理 |
| TCP TIME_WAIT 佔 socket slot | `CONFIG_LWIP_TCP_MSL=3000`（6s TIME_WAIT）；portal server 用 SO_LINGER 送 RST |

### Portal 雙模式架構

| WiFi 狀態 | Portal 模式 | Bot task | 說明 |
|-----------|------------|----------|------|
| 已連線 | STA（不切 WiFi） | pause + cmd queue | 非阻塞，外網可用 |
| 未連線 | 純 AP | stop（釋放 DRAM） | 配網用，無外網 |

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

## AI 文件同步規範
- `AGENTS.md` 與 `CLAUDE.md` 需同步維護；凡共通規範變更，兩份文件必須同次更新。
- 若僅更新其中一份，必須在該文件標註「不同步原因」與適用範圍（工具專屬差異）。

## Gotchas
- 開發類 gotchas 由 `m5stack-core2-dev` 維護（避免與 `AGENTS.md` 重複）。
- 燒錄/連線類 gotchas 由 `m5stack-core2-flash-agent` 維護（例如 monitor 鎖 port）。
- `AGENTS.md` 僅保留分工與路由規則，不再重複列細節表。
- 連線判讀規則：`WiFi connected (STA)` 不等於 `Provisioning Portal active (AP/HTTP)`；診斷 Portal 頁面時不得只憑 STA 已連線判定正常。
- Portal 現為 STA/AP 雙模式：已連 WiFi 時不切換 WiFi 模式（STA 直接服務），未連 WiFi 時用純 AP。**不可使用 APSTA 模式**。

## AI 協作分工（去重）
- 功能開發、程式修改與邏輯除錯一律使用 `m5stack-core2-dev`。
- `components/portal_backend/portal` 網頁調整優先使用 `m5stack-portal-web-dev`；若需 firmware/API 行為變更再轉交 `m5stack-core2-dev`。
- 連線、燒錄、監看與 log 取得一律使用 `m5stack-core2-flash-agent`。
- `m5stack-core2-flash-agent` 在燒錄阻塞時可最小修改 `flash.sh`（限連線/燒錄路徑），不得延伸到韌體功能邏輯。
- `m5stack-core2-dev` 需要實機 log 時，先切 `m5stack-core2-flash-agent` 取得結果，再回 `m5stack-core2-dev` 續修。
- 具體流程與守則以各自的 skill/agent 文件為準，`AGENTS.md` 不重複維護其細節。

## 路由可觀測性
- 每個任務開始時，第一則進度訊息必須明確標示：`Routing: <skill-name>`。
- 若任務中途改派（例如 `m5stack-core2-dev` 轉 `m5stack-core2-flash-agent`），需再補一則路由切換訊息。
- 計劃中的驗證步驟（build/flash/monitor）也需標記由哪個 skill 執行，避免實作時遺漏路由切換。
- 當 `Routing: m5stack-portal-web-dev` 時，需補一行：`Scope: portal static files only; No firmware/API changes`。
