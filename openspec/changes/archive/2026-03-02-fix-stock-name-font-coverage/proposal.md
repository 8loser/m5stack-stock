## Why

`lv_font_noto_tc_14.c` / `lv_font_noto_tc_16.c` 只包含 ~70 個 UI 標籤專用的繁體漢字，TWSE 回傳的股票名稱（如台積電、聯發科）所需的字元不在 glyph table 中，導致 Dashboard 顯示亂碼（空格或方塊）。字型生成腳本需要自動從 TWSE API 取得股票名稱並擴充字符集。

## What Changes

- **新增** `tools/fonts/fetch_stock_chars.py`：只負責解析 TWSE JSON、提取唯一漢字，合併 UI SYMBOLS 後輸出到 stdout
- **修改** `tools/fonts/generate_fonts.sh`：先用 `curl` 抓 TWSE JSON，再呼叫 Python script 解析擴充字符集；下載/解析失敗時 fallback 到原本的 UI SYMBOLS，行為向下相容

## Capabilities

### New Capabilities

（無）

### Modified Capabilities

- `cjk-font-system`：字型子集來源從「靜態 UI SYMBOLS 字串」擴充為「UI SYMBOLS + 動態抓取的 TWSE 股票名稱漢字」；新增字型生成腳本抓檔與解析流程

## Impact

- `tools/fonts/generate_fonts.sh`（修改）
- `tools/fonts/fetch_stock_chars.py`（新增）
- `components/ui/fonts/lv_font_noto_tc_14.c`（重新生成，體積約從 105KB → 500KB）
- `components/ui/fonts/lv_font_noto_tc_16.c`（重新生成，體積約從 125KB → 600KB）
- 依賴：`python3`（標準庫 `json`，不需 pip install）、`curl`、`lv_font_conv`（已有）
- Flash 使用量增加約 +900KB，16MB flash 可承受
