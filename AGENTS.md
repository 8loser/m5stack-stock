# 倉庫貢獻指南

## 專案結構與模組規劃
本專案是 M5Stack Core2 的 ESP-IDF（`v5.1.x`）韌體專案。

- `main/`：程式入口與共用設定（`main.c`、`app_config.h`）
- `components/`：功能模組（board HAL、WiFi、storage、TWSE client、AI provider、scheduler、UI）
- `examples/`：遠端設定範例（`token_config_example.json`、`prompt_config_example.json`）
- `build/`：建置產物（請勿手動修改）
- 根目錄設定：`CMakeLists.txt`、`partitions.csv`、`sdkconfig*`、`idf_component.yml`

## 建置、測試與開發指令
優先使用封裝腳本（自動偵測 Docker/Podman）：

- `./flash.sh`：建置 + 燒錄 + 序列埠監看
- `./flash.sh --build-only`：只編譯韌體
- `./flash.sh --flash-only [/dev/ttyACM0]`：只燒錄既有建置結果（可省略序列埠，自動偵測）
- `./flash.sh --app-flash [/dev/ttyACM0]`：只燒錄 app 分區（開發迭代較快）
- `./flash.sh --monitor [/dev/ttyACM0]`：只開啟序列埠監看（可省略序列埠，自動偵測）
- `./flash.sh --erase [/dev/ttyACM0]`：清除整顆 Flash 後再燒錄（可省略序列埠，自動偵測）
- `./flash.sh --shell`：進入 ESP-IDF 容器除錯

## 程式風格與命名慣例
- 語言：C（ESP-IDF + FreeRTOS + LVGL）。
- 縮排：4 個空白；函式大括號另起一行。
- 函式與變數使用 `snake_case`（例：`ui_manager_update_quote`）。
- 巨集與常數使用 `UPPER_SNAKE_CASE`（例：`MAX_STOCK_COUNT`）。
- 模組 API 以元件前綴命名（如 `wifi_manager_*`、`ai_provider_*`）。
- 硬體操作集中在 `components/board/`，上層避免直接呼叫裝置驅動。

## 測試準則
目前尚無獨立自動化測試框架，請至少完成：

1. `./flash.sh --build-only`（編譯必須通過）
2. 實機驗證主要流程（UI、WiFi、報價抓取、AI、排程）
3. `./flash.sh --monitor` 檢查執行期 log 與警告

若變更邏輯較大，請在 PR 附上可重現的手動測試清單。

## Commit 與 Pull Request 規範
- Commit 建議使用前綴式訊息；目前歷史有 `init:`，建議採用 `feat:`、`fix:`、`refactor:`、`docs:`、`chore:`。
- 每個 Commit 聚焦單一目的，避免把重構與行為變更混在一起。
- PR 需說明變更內容與原因。
- PR 需列出已執行的軟硬體驗證步驟。
- 有對應議題時請附上 issue 連結。
- 涉及 UI 變更請附截圖或錄影。

## 安全與設定提醒
- 不要提交真實 API Key、WiFi 密碼或私人 Token URL。
- 遠端設定請以 `examples/` 範本為基礎，機密資訊放在私有端點。
