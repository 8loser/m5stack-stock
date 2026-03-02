# M5Stack Core2 台股監測韌體

本專案是以 **ESP-IDF v5.1.x** 開發的 M5Stack Core2 韌體，提供：

- 台股（TWSE）報價輪詢
- AI 分析（Gemini / Claude / OpenAI）
- 手機配網 Portal（SoftAP + Web）
- 觸控 UI（Dashboard / Portal / Log / Info / Settings / HW Test）

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
# 1) 測試抓取 TWSE 股票名稱字符集（預設會先做 SSL 驗證）
curl --fail --silent --show-error --location \
  https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL \
  | python3 tools/fonts/fetch_stock_chars.py --input-json -

# 2) 產生 14px / 16px 字型
./generate_fonts.sh

# 3) 檢查字型大小（擴充後通常會明顯增加）
wc -c components/ui/fonts/lv_font_noto_tc_14.c components/ui/fonts/lv_font_noto_tc_16.c
```

說明：

- `generate_fonts.sh` 會先用 `curl` 下載 TWSE JSON，再交給 Python 解析字符集。
- 若遇到憑證驗證錯誤，腳本會自動改用 `curl -k` 重試。
- 若下載或解析失敗，腳本會直接中止，不會繼續執行 `lv_font_conv`。
- 字型工具集中放在 `tools/fonts/`。
- 每次執行會同步更新 `tools/fonts/twse_symbols.txt`（實際送給 `lv_font_conv --symbols` 的字元集）。
- 完成後請重新執行 `./flash.sh --build-only` 或 `./flash.sh`。

## 目前 UI 與操作

### 底部虛擬按鍵（觸控區）

- 左鍵：切到上一頁（輪詢頁面：`Dashboard -> Log -> Info -> Settings -> HW Test`）
- 中鍵：進入 `Portal`
- 右鍵：切到下一頁（同上輪詢）
- 在 `Portal` 頁面時，任一底部鍵都會回到 `Dashboard`

### 頁面

- `Dashboard`：顯示最多 5 檔股票卡片（代號/名稱、價格、漲跌幅、最後更新時間）
- `Portal`：顯示配網 QRCode 與 AP 資訊，進入頁面時會啟動配網 Portal，離開時會關閉
- `Log`：顯示系統事件（STOCK / WIFI / AI / SYS）
- `Info`：顯示裝置資訊、WiFi 狀態、AI Provider 狀態、排程摘要
- `Settings`：切換報價更新間隔（1 / 5 / 10 分鐘）
- `HW Test`：提供震動與嗶聲硬體測試按鈕

### 頂部狀態列

所有頁面共用：

- 時間（12 小時制）
- 中央 screen 標題（`Dashboard / Portal / Log / Info / Settings / HW Test`）
  - `Portal` 頁面固定顯示：`Portal`
- WiFi 圖示（連線/連線中/離線）
- 電量百分比

## Provisioning Portal（手機配網）

Portal 啟動後會提供：

- AP SSID：`Core2-Setup`
- 密碼：`core2wifi`
- URL：`http://192.168.4.1`

Web Portal 分成 3 個分頁：

- `WiFi`：掃描 AP、提交 SSID/密碼
- `AI Provider`：設定 Gemini / Claude / OpenAI API Key 與 Prompt Template
- `Stocks`：管理股票清單（TWSE only，最多 10 檔）

Stocks API 驗證規則：

- 代號必須為 4 位數字
- 新增時會先向 TWSE 驗證，且僅接受 `tse`
- 常見錯誤碼：`invalid_format`、`duplicate_symbol`、`limit_exceeded`、`not_found_or_not_tse`、`validate_failed`、`not_found`

## 排程與資料流

- Scheduler 依設定定時抓取 TWSE 報價
- 報價資料推送到 UI queue 後更新 Dashboard
- AI 分析任務會依排程觸發，結果寫入 log queue
- 股票清單由 NVS 保存，Portal 異動後會 reload 到 scheduler

## 專案結構

- `main/`：入口與全域設定
- `components/board/`：硬體抽象層（LCD/Touch/AXP192/RTC/音效/震動）
- `components/ui/`：LVGL UI（screens/widgets/ui manager）
- `components/device_server/`：STA 連線、SoftAP、Portal HTTP 服務
- `components/twse_client/`：TWSE API 抓價與代號驗證
- `components/ai_provider/`：AI provider 封裝
- `components/scheduler/`：報價/AI 排程與睡眠策略
- `components/storage/`：NVS 設定儲存
- `openspec/`：需求與變更規格（OpenSpec）

## 注意事項

- 請勿提交真實 API Key、WiFi 密碼或私人端點
- `build/` 為建置產物，請勿手動修改
- 若變更 UI/流程，請同步更新本 README
