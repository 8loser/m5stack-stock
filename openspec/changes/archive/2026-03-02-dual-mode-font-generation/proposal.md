## Why

目前字型產生流程強制下載 TWSE 資料，導致在離線環境、CI 無網路或 API 暫時不可用時無法產生字型。另一次需求是同時保留「更新字集」與「可重現離線建置」兩種工作流。

## What Changes

- 新增 UI 字元掃描腳本，自動從 UI 原始碼產生 `tools/fonts/ui_symbols.txt`。
- 調整 `tools/fonts/generate_fonts.sh` 為雙模式：
  - `--online`：抓取 TWSE 後更新 `twse_symbols.txt`，再產生字型。
  - `--offline`（預設）：直接使用既有 `twse_symbols.txt`，不連網產生字型。
- 調整字型產生流程：每次先更新 `ui_symbols.txt`，再合併 `ui_symbols.txt` + `twse_symbols.txt` 作為 `lv_font_conv --symbols` 輸入。
- 明確定義離線模式下 `twse_symbols.txt` 缺失或空檔的失敗行為。

## Capabilities

### New Capabilities
- None

### Modified Capabilities
- `cjk-font-system`: 字型產生腳本改為支援 online/offline 兩種符號來源策略，並新增 UI 字元自動萃取流程。

## Impact

- Affected code:
  - `tools/fonts/generate_fonts.sh`
  - `tools/fonts/fetch_stock_chars.py`（保留在線更新流程使用）
  - `tools/fonts/ui_symbols.txt`（由腳本產生）
  - `tools/fonts/extract_ui_symbols.py`（新增）
- Affected system: CJK 字型子集產生流程與開發/CI 操作指令。
- No firmware runtime API breaking changes.
