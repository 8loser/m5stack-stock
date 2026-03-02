## ADDED Requirements

### Requirement: 字型字符集自動擴充腳本
專案 SHALL 包含 `tools/fonts/fetch_stock_chars.py`，該腳本 SHALL 從輸入的 TWSE JSON 檔案提取股票名稱，抽取 CJK Unified Ideographs（U+4E00–U+9FFF）範圍內的唯一漢字，合併 UI SYMBOLS 後輸出到 stdout。腳本 SHALL 僅使用 Python 標準庫（`json`），不需要第三方套件。若 JSON 無法解析或缺少名稱資料，腳本 SHALL 以非零狀態碼退出。

#### Scenario: 成功取得股票名稱
- **WHEN** 執行 `python3 fetch_stock_chars.py --input-json <twse.json>`，且 JSON 內容有效
- **THEN** stdout 輸出合併 UI SYMBOLS 與所有股票名稱漢字的唯一字元字串，退出碼為 0

#### Scenario: JSON 輸入無效
- **WHEN** 執行 `python3 fetch_stock_chars.py --input-json <twse.json>`，且 JSON 非法或缺少可用名稱欄位
- **THEN** 腳本以非零狀態碼退出，不輸出任何內容到 stdout

## MODIFIED Requirements

### Requirement: 使用 lv_font_conv 產生 Noto Sans TC 子集字型
系統 SHALL 使用 `lv_font_conv --no-compress --no-prefilter --bpp 4 --format lvgl` 從 Noto Sans TC TTF 產生子集字型 .c 檔，包含 ASCII 0x20-0x7F 及 UI 所需的繁體中文字元與所有 TWSE 上市股票名稱所需漢字。

#### Scenario: 產生 14px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 14`
- **THEN** 產生 `lv_font_noto_tc_14.c`，包含 ASCII、UI 字元及股票名稱漢字，無壓縮

#### Scenario: 產生 16px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 16`
- **THEN** 產生 `lv_font_noto_tc_16.c`，包含 ASCII、UI 字元及股票名稱漢字，無壓縮

### Requirement: 字型產生腳本
專案 SHALL 包含字型產生腳本（`generate_fonts.sh`）。腳本 SHALL 先下載 TWSE JSON，再呼叫 `fetch_stock_chars.py` 取得擴充字符集；若下載或解析失敗，SHALL fallback 至靜態 UI SYMBOLS，確保 build 不中斷。腳本 SHALL 於執行時顯示實際使用的字符集來源。

#### Scenario: 擴充字符集可用
- **WHEN** 執行 `./generate_fonts.sh`，且 TWSE JSON 下載與解析成功
- **THEN** 以擴充字符集（UI SYMBOLS + 股票名稱漢字）產生字型，並顯示「使用擴充字符集」訊息

#### Scenario: 擴充字符集不可用（fallback）
- **WHEN** 執行 `./generate_fonts.sh`，且 TWSE JSON 下載或解析失敗
- **THEN** 以原始靜態 UI SYMBOLS 產生字型，並顯示 warning 訊息，build 正常完成
