## 1. 新增 fetch_stock_chars.py

- [x] 1.1 建立 `tools/fonts/fetch_stock_chars.py`
- [x] 1.2 實作 `UI_SYMBOLS` 常數（從 `generate_fonts.sh` line 11 抄入）
- [x] 1.3 實作 `fetch_stock_chars.py` JSON 解析流程：讀取本地 TWSE JSON，提取名稱欄位（含多鍵嘗試）
- [x] 1.4 在 shell 端以 `curl` 下載 TWSE JSON，並把檔案交給 Python 解析（不在 Python 直接連網）
- [x] 1.5 實作 CJK 字元過濾（U+4E00–U+9FFF），提取唯一漢字並合併 UI_SYMBOLS
- [x] 1.6 任一 API 失敗時 catch exception 並跳過（不中止），兩者均失敗時 `sys.exit(1)`
- [x] 1.7 手動測試：`python3 fetch_stock_chars.py --input-json -` 輸出合理字元串（含台積電的「台」「積」等字）

## 2. 修改 generate_fonts.sh

- [x] 2.1 在 `SYMBOLS=...` 宣告後加入呼叫 Python script 的 block：成功則覆蓋 `SYMBOLS`，失敗則印 warning 並保留原值
- [x] 2.2 手動測試：執行 `./generate_fonts.sh`，確認顯示「使用擴充字符集」並產生新的 .c 檔
- [x] 2.3 確認新字型 .c 檔體積明顯增加（預期 lv_font_noto_tc_14.c 從 ~105KB 增至 ~450–550KB）

## 3. Build 驗證

- [x] 3.1 執行 `./flash.sh --build-only`，確認編譯無錯誤
- [x] 3.2 執行 `./flash.sh`，燒錄至 Core2
- [x] 3.3 在 Dashboard 確認監控的股票名稱正確顯示（無亂碼、無方塊）
- [x] 3.4 確認其他頁面（Log、Info、Settings）中文標籤顯示正常（regression check）
