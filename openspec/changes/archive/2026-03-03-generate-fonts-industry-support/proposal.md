## Why

`generate_fonts.sh` 的 online 模式目前只抓股票名稱 CJK 字元，未包含產業別名稱（如「半導體業」、「電子零組件業」）。這些字元在 Dashboard 上會顯示為方塊，需要補入字型。

## What Changes

- `extract_ui_symbols.py`：`--src` 改支援多路徑（`action="append"`），允許同時掃描多個 components 目錄
- `generate_fonts.sh`：兩種模式都新增掃描 `components/twse_client/`，補入 `s_industry_map` 的 36 個硬編碼產業名稱
- `generate_fonts.sh`（online 模式）：新增第二個 TWSE API fetch，抓取公司產業別資料，提取 CJK 字元存入 `industry_symbols.txt`，合併進最終字元集
- `fetch_stock_chars.py`：新增 `--mode industry` 支援，加入 `TWSE_INDUSTRY_KEYS`（欄位名）與 `INDUSTRY_CODE_MAP`（產業代碼對照，Python 版 `s_industry_map`）

## Capabilities

### New Capabilities

- `font-industry-chars`: 字型產生器支援產業別 CJK 字元——靜態從 C 原始碼掃描，online 模式另加動態 TWSE API 抓取

### Modified Capabilities

（無 spec 層級的行為變更；工具行為改進不影響現有 openspec/specs）

## Impact

- `tools/fonts/extract_ui_symbols.py`
- `tools/fonts/generate_fonts.sh`
- `tools/fonts/fetch_stock_chars.py`
- 生成的 `tools/fonts/industry_symbols.txt`（新增，自動產生，建議加入 .gitignore）
- 無 ESP32 韌體程式碼變更
