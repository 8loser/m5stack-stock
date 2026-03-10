# cjk-font-system Specification

## Purpose
為 UI 提供可維護的繁體中文字型子集機制，確保中文字可正確顯示並控制韌體體積。
## Requirements
### Requirement: UI 字元掃描腳本產生 ui_symbols.txt
專案 SHALL 包含 `tools/fonts/extract_ui_symbols.py`。該腳本 SHALL 掃描指定來源目錄的 C/H 原始碼字串常量，抽取非 ASCII 字元並去重後輸出到 `tools/fonts/ui_symbols.txt`，供字型產生流程使用。`generate_fonts.sh` SHALL 至少掃描 `components/ui`、`components/twse_client`、`main` 三個來源目錄。

#### Scenario: 成功掃描多來源原始碼
- **WHEN** 執行 UI 字元掃描腳本，且輸入目錄存在可解析字串
- **THEN** 產生非空且去重後的 `ui_symbols.txt`

#### Scenario: 輸入來源無法掃描
- **WHEN** 執行 UI 字元掃描腳本，且輸入目錄不存在或無法讀取
- **THEN** 腳本以非零狀態碼退出並輸出錯誤訊息

### Requirement: 字型字符集自動擴充腳本
專案 SHALL 包含 `tools/fonts/fetch_stock_chars.py`，該腳本 SHALL 從輸入的 TWSE JSON 檔案提取股票名稱，抽取 CJK Unified Ideographs（U+4E00–U+9FFF）範圍內的唯一漢字並輸出到 stdout。腳本 SHALL 僅使用 Python 標準庫（`json`），不需要第三方套件。若 JSON 無法解析或缺少名稱資料，腳本 SHALL 以非零狀態碼退出。

#### Scenario: 成功取得股票名稱
- **WHEN** 執行 `python3 fetch_stock_chars.py --input-json <twse.json>`，且 JSON 內容有效
- **THEN** stdout 輸出所有股票名稱漢字的唯一字元字串，退出碼為 0

#### Scenario: JSON 輸入無效
- **WHEN** 執行 `python3 fetch_stock_chars.py --input-json <twse.json>`，且 JSON 非法或缺少可用名稱欄位
- **THEN** 腳本以非零狀態碼退出，不輸出任何內容到 stdout

### Requirement: 使用 lv_font_conv 產生 Noto Sans TC 子集字型
系統 SHALL 使用 `lv_font_conv --no-compress --no-prefilter --bpp 4 --format lvgl` 從 Noto Sans TC TTF 產生子集字型 .c 檔，包含 ASCII 0x20-0x7F 及 UI 所需的繁體中文字元與所有 TWSE 上市股票名稱所需漢字。

#### Scenario: 產生 14px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 14`
- **THEN** 產生 `lv_font_noto_tc_14.c`，包含 ASCII、UI 字元及股票名稱漢字，無壓縮

#### Scenario: 產生 16px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 16`
- **THEN** 產生 `lv_font_noto_tc_16.c`，包含 ASCII、UI 字元及股票名稱漢字，無壓縮

### Requirement: 字型檔案放置於 components/ui/fonts/
子集字型 .c 檔 SHALL 放置於 `components/ui/fonts/` 目錄，並在 `components/ui/CMakeLists.txt` 的 SRCS 中列出。

#### Scenario: Build 包含字型檔案
- **WHEN** 執行 `idf.py build`
- **THEN** `lv_font_noto_tc_14.c` 和 `lv_font_noto_tc_16.c` 被編譯進韌體

### Requirement: UI_FONT_TEXT_DEFAULT 為 Noto Sans TC 14px
`ui_compat.h` 中 `UI_FONT_TEXT_DEFAULT` SHALL 定義為 `lv_font_noto_tc_14`，作為所有 UI 文字的預設字型。

#### Scenario: 預設字型為 Noto Sans TC 14px
- **WHEN** 任一 UI 元件使用 `UI_FONT_TEXT_DEFAULT`
- **THEN** 該元件的字型為 `lv_font_noto_tc_14`（14px，含 ASCII + 繁中子集）

### Requirement: ui_font.h 提供字型 extern 宣告
`components/ui/include/ui_font.h` SHALL 提供 `lv_font_noto_tc_14` 和 `lv_font_noto_tc_16` 的 extern 宣告，供各 screen 檔案引用。

#### Scenario: Screen 檔案引用字型
- **WHEN** screen_dashboard.c include `ui_font.h`
- **THEN** 可使用 `&lv_font_noto_tc_14` 和 `&lv_font_noto_tc_16`

### Requirement: 字型產生腳本
專案 SHALL 包含字型產生腳本（`generate_fonts.sh`）。腳本 SHALL 先執行 UI 字元掃描流程產生 `ui_symbols.txt`，再依模式處理 TWSE 字元來源：`--online` 模式 SHALL 下載 TWSE JSON 並更新 `twse_symbols.txt`；`--offline` 模式 SHALL 直接使用既有 `twse_symbols.txt`。腳本在未指定模式時 SHALL 預設使用 `--offline`。最終 SHALL 合併 `ui_symbols.txt` 與 `twse_symbols.txt`，且在 `industry_symbols.txt` 存在且非空時一併合併，作為 `lv_font_conv --symbols` 輸入，並產生 14/16 字型。

#### Scenario: 預設離線模式產生字型
- **WHEN** 執行 `./generate_fonts.sh` 且 `twse_symbols.txt` 存在且非空
- **THEN** 腳本不進行網路下載，直接使用現有 `twse_symbols.txt` 與新產生的 `ui_symbols.txt` 合併產生字型

#### Scenario: 線上模式更新 TWSE 後產生字型
- **WHEN** 執行 `./generate_fonts.sh --online` 且 TWSE JSON 下載與解析成功
- **THEN** 腳本更新 `twse_symbols.txt`，再以合併字符集產生字型

#### Scenario: 離線模式缺少 TWSE 字集檔
- **WHEN** 執行 `./generate_fonts.sh --offline` 且 `twse_symbols.txt` 不存在或為空
- **THEN** 腳本以非零狀態碼退出，且不執行 `lv_font_conv`

#### Scenario: 線上模式更新失敗
- **WHEN** 執行 `./generate_fonts.sh --online` 且 TWSE JSON 下載或解析失敗
- **THEN** 腳本以非零狀態碼退出，且不執行 `lv_font_conv`

### Requirement: 心跳 emoji 字元納入 UI 字型字符集
字型產生流程 SHALL 將心跳指標使用的 emoji 字元納入 `ui_symbols.txt` 來源集合，並隨 `generate_fonts.sh` 輸出到 `lv_font_noto_tc_14.c` 與 `lv_font_noto_tc_16.c`。

#### Scenario: 重新產生字型後含心跳 emoji
- **WHEN** 執行 `./generate_fonts.sh`
- **THEN** 產生的 Noto TC 子集字型包含心跳 emoji 字元

### Requirement: 心跳字元渲染失敗時 fallback
當目標字型無法正確渲染心跳 emoji 時，UI SHALL fallback 使用 ASCII `<3` 作為正常心跳符號，避免顯示缺字方框。

#### Scenario: emoji 可渲染
- **WHEN** 字型包含且可渲染心跳 emoji
- **THEN** status bar 正常狀態顯示 emoji 心跳符號

#### Scenario: emoji 不可渲染
- **WHEN** 字型缺少心跳 emoji 或渲染結果為缺字
- **THEN** status bar 正常狀態改顯示 `<3`
