# 倉庫貢獻指南

## 專案結構與模組規劃
本專案是 M5Stack Core2 的 ESP-IDF（`v5.1.x`）韌體專案。

- `main/`：程式入口與共用設定（`main.c`、`app_config.h`）
- `components/`：功能模組（board HAL、app_core event bus、network_portal façade、wifi_manager、portal_backend、storage、TWSE client、AI provider、scheduler、UI）
- `docs/hardware_quick_ref.md`：低 token 硬體速查（AI 開發預設先讀）
- `docs/hardware_core2_reference.md`：Core2 官方規格、PinMap 與完整對照（需要細節時再查）
- `docs/twse_api_fields.md`：TWSE `getStockInfo.jsp` 回傳欄位對照與本專案解析規則
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

## AI 文件同步規範
- `AGENTS.md` 與 `CLAUDE.md` 需同步維護；凡共通規範變更，兩份文件必須同次更新。
- 若僅更新其中一份，必須在該文件標註「不同步原因」與適用範圍（工具專屬差異）。

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
| Portal 前端維護位置 | Portal 前端已拆為多檔：`components/portal_backend/portal/index.html` + `portal.css` + `portal_bootstrap.js` + `tab_*.js`；C 端由 `EMBED_TXTFILES` 內嵌回傳，避免在 `*.c` 內維護長 HTML/JS 字串 |

## AI 開發上下文最小化
- 本專案以 AI 協作為主，預設先讀 `docs/hardware_quick_ref.md`，避免每次載入完整硬體文件。
- 只有在需要 pinmap 背景、官方連結或完整規格時，才展開 `docs/hardware_core2_reference.md`。
- 涉及 TWSE API 欄位語意、回傳相容性或 parser 行為時，優先查 `docs/twse_api_fields.md`。
