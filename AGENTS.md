# 倉庫貢獻指南

## 專案結構與模組規劃
本專案是 M5Stack Core2 的 ESP-IDF（`v5.1.x`）韌體專案。

- `main/`：程式入口與共用設定（`main.c`、`app_config.h`）
- `components/`：功能模組（board HAL、app_core event bus、network_portal façade、wifi_manager、portal_backend、storage、TWSE client、AI provider、scheduler、UI）
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

## Commit 與 Pull Request 規範
- Commit 建議使用前綴式訊息；目前歷史有 `init:`，建議採用 `feat:`、`fix:`、`refactor:`、`docs:`、`chore:`。
- 每個 Commit 聚焦單一目的，避免把重構與行為變更混在一起。
- PR 需說明變更內容與原因。
- PR 需列出已執行的軟硬體驗證步驟。
- 有對應議題時請附上 issue 連結。
- 涉及 UI 變更請附截圖或錄影。

## 安全與設定提醒
- 不要提交真實 API Key、WiFi 密碼或私人 Token URL。
- 遠端設定範本目前不在此 repo，機密資訊與實際設定請放在私有端點或私有配置來源。

## AI 文件同步規範
- `AGENTS.md` 與 `CLAUDE.md` 需同步維護；凡共通規範變更，兩份文件必須同次更新。
- 若僅更新其中一份，必須在該文件標註「不同步原因」與適用範圍（工具專屬差異）。

## Gotchas
- 開發類 gotchas 由 `m5stack-core2-dev` 維護（避免與 `AGENTS.md` 重複）。
- 燒錄/連線類 gotchas 由 `m5stack-core2-flash-agent` 維護（例如 monitor 鎖 port）。
- `AGENTS.md` 僅保留分工與路由規則，不再重複列細節表。

## AI 協作分工（去重）
- 功能開發、程式修改與邏輯除錯一律使用 `m5stack-core2-dev`。
- 連線、燒錄、監看與 log 取得一律使用 `m5stack-core2-flash-agent`。
- `m5stack-core2-dev` 需要實機 log 時，先切 `m5stack-core2-flash-agent` 取得結果，再回 `m5stack-core2-dev` 續修。
- 具體流程與守則以各自的 skill/agent 文件為準，`AGENTS.md` 不重複維護其細節。
