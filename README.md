# M5Stack Core2 台股監測韌體

本專案是以 **ESP-IDF v5.1.x** 開發的 M5Stack Core2 韌體，提供：

- 台股（TWSE）報價輪詢
- AI 分析（Gemini / Claude / OpenAI）
- 手機配網 Portal（SoftAP + Web）
- 觸控 UI（Dashboard / Portal / Log / Resource / Settings / HW Test）

## 硬體需求

- M5Stack Core2（ESP32）
- USB-C 傳輸線
- WiFi 網路

## 開發環境

建議使用專案內建腳本，不需在主機直接安裝 ESP-IDF：

- Docker 或 Podman（腳本會自動偵測）

## 快速開始

```bash
cd m5stack_stock

# build + flash + monitor（預設）
./flash.sh

# 指定序列埠
./flash.sh /dev/ttyACM0

# 僅編譯
./flash.sh --build-only
```

## 常用指令

```bash
# 只燒錄既有 build
./flash.sh --flash-only /dev/ttyACM0

# 只燒錄 app 分區（開發迭代較快）
./flash.sh --app-flash /dev/ttyACM0

# 只看序列埠輸出
./flash.sh --monitor /dev/ttyACM0

# 清除 flash 後重燒
./flash.sh --erase /dev/ttyACM0

# 進入 ESP-IDF 容器 shell
./flash.sh --shell
```

## 字型擴充與重生

當股票名稱出現缺字或方塊時，可重生 Noto Sans TC 子集字型：

```bash
# 1) （可選）線上更新 TWSE 股票名稱字集
curl --fail --silent --show-error --location \
  https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL \
  | python3 tools/fonts/fetch_stock_chars.py --input-json -

# 2) 產生 14px / 16px 字型（預設離線，不連網）
tools/fonts/generate_fonts.sh --offline

# 3) 線上模式：先更新 twse_symbols.txt 再產生字型
tools/fonts/generate_fonts.sh --online

# 4) 檢查字型大小（擴充後通常會明顯增加）
wc -c components/ui/fonts/lv_font_noto_tc_14.c components/ui/fonts/lv_font_noto_tc_16.c
```

說明：

- `generate_fonts.sh` 支援 `--offline`（預設）與 `--online`。
- 每次執行都會先掃描 `components/ui` 字串常量，自動更新 `tools/fonts/ui_symbols.txt`。
- `--offline` 直接使用既有 `tools/fonts/twse_symbols.txt`；若檔案缺失或為空會失敗退出，且不執行 `lv_font_conv`。
- `--online` 會先下載 TWSE JSON 更新 `tools/fonts/twse_symbols.txt`（若憑證驗證失敗會用 `curl -k` 重試）。
- 腳本會合併 `ui_symbols.txt` 與 `twse_symbols.txt` 作為 `lv_font_conv --symbols` 輸入。
- 字型工具集中放在 `tools/fonts/`。
- 完成後請重新執行 `./flash.sh --build-only` 或 `./flash.sh`。

## 目前 UI 與操作

### 底部虛擬按鍵（觸控區）

- 左鍵：切到上一頁（輪詢頁面：`Dashboard -> Log -> Resource -> Settings -> HW Test`）
- 中鍵：進入 `Portal`
- 右鍵：切到下一頁（同上輪詢）
- 在 `Portal` 頁面時，任一底部鍵都會回到 `Dashboard`

### 自動返回 Dashboard

- 在非 `Portal`、非 `Dashboard` 頁面，若 10 秒內沒有觸控，會自動返回 `Dashboard`
- 熄屏期間不進行閒置超時判定
- 熄屏時若目前不在 `Dashboard`，會預先切回 `Dashboard`；因此亮屏第一時間會直接顯示 `Dashboard`

### 頁面

- `Dashboard`：顯示最多 5 檔股票卡片（代號/名稱、價格、漲跌幅、最後更新時間）
- `Portal`：顯示配網 QRCode 與 AP 資訊，進入頁面時會啟動配網 Portal，離開時會關閉
- `Log`：顯示系統事件（STOCK / WIFI / AI / SYS）
- `Resource`：顯示 Heap current 與 Stack peak（LVGL）使用率 progress bar
- `Settings`：切換報價更新間隔（1 / 5 / 10 分鐘）
- `HW Test`：提供震動與嗶聲硬體測試按鈕

### 頂部狀態列

所有頁面共用：

- 主流程心跳（時間左側）
  - 正常：顯示 `♡`，每秒閃爍一次
  - 異常：顯示 `!`（停止閃爍）
- 時間（12 小時制）
- 中央 screen 標題（`Dashboard / Portal / Log / Resource / Settings / HW Test`）
  - `Portal` 頁面固定顯示：`Portal`
- WiFi 圖示（連線/連線中/離線）
- 電量百分比

## Provisioning Portal（手機配網）

Portal 啟動後會提供：

- AP SSID：`Core2-Setup`
- 密碼：`core2wifi`
- URL：`http://192.168.4.1`

狀態判讀提醒：

- `WiFi connected`（STA 有內網 IP）不代表 Portal 一定已啟動；Portal 是否可用應另外確認 `AP/HTTP` 是否 active。

Web Portal 分成 6 個分頁：

- `WiFi`：掃描 AP、提交 SSID/密碼
- `AI Provider`：設定 Gemini / Claude / OpenAI API Key 與 Prompt Template
- `Telegram`：設定 Bot Token / Chat ID，並可送測試訊息
- `Stocks`：管理股票清單（TWSE only，最多 15 檔）
- `At Time`：定時 AI 任務（可設定啟用、時間、星期與 Prompt，寫入裝置）
- `Interval`：固定間隔 AI 觸發 UI（目前為前端示意，後端尚未實作）

Portal 前端檔案位置：

- `components/portal_backend/portal/index.html`：頁面結構
- `components/portal_backend/portal/portal.css`：樣式
- `components/portal_backend/portal/portal_bootstrap.js`：初始化與 tab lazy-load
- `components/portal_backend/portal/tab_*.js`：各分頁邏輯（WiFi/AI/Telegram/Stocks/At Time/Interval）

Stocks API 驗證規則：

- 代號必須為 4 位數字
- 新增時會先向 TWSE 驗證，且僅接受 `tse`
- 常見錯誤碼：`invalid_format`、`duplicate_symbol`、`limit_exceeded`、`not_found_or_not_tse`、`validate_failed`、`not_found`

## 排程與資料流

- Scheduler Service 依設定定時抓取 TWSE 報價
- 報價資料推送到 UI queue 後更新 Dashboard
- AI 分析任務會依排程觸發，結果寫入 log queue
- 股票清單由 NVS 保存，Portal 異動後會 reload 到 scheduler service

## 專案結構

- `main/`：入口與全域設定
- `components/app_core/`：應用層事件匯流排（跨模組解耦）
- `components/board/`：硬體抽象層（LCD/Touch/AXP192/RTC/音效/震動）
- `components/ui/`：LVGL UI（screens/widgets/ui manager）
- `components/network_portal/`：對外 façade（統一 network API）
- `components/wifi_manager/`：STA 連線、狀態機、AP 掃描
- `components/portal_backend/`：SoftAP + Portal HTTP 服務
- `components/twse_client/`：TWSE API 抓價與代號驗證
- `components/ai_provider/`：AI provider 封裝
- `components/scheduler_service/`：報價排程與休市睡眠策略
- `components/storage/`：NVS 設定儲存
- `openspec/`：需求與變更規格（OpenSpec）

## 注意事項

- 請勿提交真實 API Key、WiFi 密碼或私人端點
- `build/` 為建置產物，請勿手動修改
- Core2 這批面板在目前驅動下顏色可能偏移，新增/調整 UI 色彩時建議先參考 `components/ui/screens/screen_dashboard.c` 的校正色（`COLOR_UP/DOWN/FLAT`）再上板確認
- 若修改 `tools/fonts/ui_symbols.txt`（例如心跳符號），需先執行 `tools/fonts/generate_fonts.sh` 重建子集字型，再編譯與燒錄
- 若變更 UI/流程，請同步更新本 README
