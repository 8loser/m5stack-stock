## 1. extract_ui_symbols.py：支援多路徑

- [x] 1.1 將 `--src` 參數改為 `action="append", dest="srcs"`；未傳時 fallback 預設 `[components/ui]`
- [x] 1.2 `collect_symbols()` 接受 `list[Path]`，對每個路徑呼叫 `iter_source_files()` 並 union 結果
- [x] 1.3 驗證：`python3 extract_ui_symbols.py --src components/ui --src components/twse_client --out /tmp/test.txt` 輸出包含「半」「導」等產業字元

## 2. fetch_stock_chars.py：新增產業別模式

- [x] 2.1 在檔案頂部加入 `TWSE_INDUSTRY_KEYS = ("產業別", "業別", "industry", "Industry")`
- [x] 2.2 加入 `INDUSTRY_CODE_MAP`（Python 版 `s_industry_map`，完整 36 筆，與 `twse_client.c` 同步）
- [x] 2.3 新增 `extract_twse_industries(data) -> list[str]`：提取產業名稱，數字代碼查表轉換，無資料時 fallback 回 `INDUSTRY_CODE_MAP.values()`
- [x] 2.4 `parse_args()` 新增 `--mode {name,industry}`，預設 `name`
- [x] 2.5 `main()` 依 `--mode` 分支：`name` 走現有路徑，`industry` 呼叫 `extract_twse_industries`
- [x] 2.6 驗證：`echo '[]' | python3 fetch_stock_chars.py --mode industry` 應輸出 fallback 的 36 個產業名稱字元（非空）

## 3. generate_fonts.sh：整合產業別抓取

- [x] 3.1 在 `extract_ui_symbols.py` 呼叫（line 98）加入 `--src "${PROJECT_ROOT}/components/twse_client"`
- [x] 3.2 新增 `INDUSTRY_SYMBOLS_FILE="${SCRIPT_DIR}/industry_symbols.txt"` 和 `TWSE_INDUSTRY_URL` 常數
- [x] 3.3 將 `TMP_INDUSTRY_JSON` 加入 `trap` 清理
- [x] 3.4 在 online 模式 STOCK_DAY_ALL fetch 之後，加入 industry API fetch（curl + `fetch_stock_chars.py --mode industry`），失敗以 `|| true` 不中斷
- [x] 3.5 合併步驟：將 `FILES_TO_MERGE` 改為陣列，若 `industry_symbols.txt` 存在且非空則加入
- [x] 3.6 更新 `print_usage` 說明 online 模式同時抓股票名稱 + 產業別
- [x] 3.7 更新最後 `echo "Generated:"` 輸出，如果 `industry_symbols.txt` 有內容則一併列出

## 4. 驗證

- [x] 4.1 **offline 驗證**：`./generate_fonts.sh --offline` → `ui_symbols.txt` 含「半導體業」所需字元
- [x] 4.2 **online 驗證**：`./generate_fonts.sh --online` → `industry_symbols.txt` 存在且非空
- [x] 4.3 **TWSE URL 確認**：執行前先 `curl -s "https://openapi.twse.com.tw/v1/company/getStockInfo" | head -c 500` 驗證格式；若 404 改用 `https://openapi.twse.com.tw/v1/opendata/t187ap03_L`
- [x] 4.4 **build 驗證**：`./flash.sh --build-only` 無錯誤
- [x] 4.5 上板驗證：Dashboard 產業別欄位不再出現方塊字
