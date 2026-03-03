# font-industry-chars Specification

## Purpose
確保字型產生流程完整涵蓋 Dashboard 產業別顯示所需字元，並在 online/offline 模式都具備穩定降級能力。

## Requirements
### Requirement: extract_ui_symbols 支援多路徑掃描
`extract_ui_symbols.py` 的 `--src` 參數 SHALL 支援多次傳入（`action="append"`），每次指定一個來源目錄；所有目錄的非 ASCII 字元 SHALL 合併後去重輸出。未傳任何 `--src` 時 SHALL 保持原預設值（`components/ui`）。

#### Scenario: 單一路徑（向後相容）
- **WHEN** 以 `--src components/ui` 呼叫
- **THEN** 輸出與原行為相同，`ui_symbols.txt` 內容不變

#### Scenario: 多路徑掃描
- **WHEN** 以 `--src components/ui --src components/twse_client` 呼叫
- **THEN** 兩個目錄的非 ASCII 字元合併去重後寫入輸出檔

#### Scenario: twse_client 產業字元被捕獲
- **WHEN** `components/twse_client/` 被加入掃描路徑
- **THEN** `s_industry_map` 中的 36 個產業名稱字元（如「半」「導」「體」「業」）出現於輸出檔

### Requirement: generate_fonts offline 模式包含靜態產業字元
`generate_fonts.sh` 在 offline 及 online 模式 SHALL 均掃描 `components/twse_client/`，確保 `s_industry_map` 的 36 個硬編碼產業名稱 CJK 字元納入 `ui_symbols.txt`。

#### Scenario: offline 模式產業字元存在
- **WHEN** 以 `--offline` 模式執行 `generate_fonts.sh`
- **THEN** 生成的 `lv_font_noto_tc_14.c` 包含「水泥工業」、「半導體業」、「電子零組件業」等產業名稱所需的所有 CJK 字元

#### Scenario: 無網路環境可正常完成
- **WHEN** 在無網路環境以 `--offline` 執行
- **THEN** 流程正常完成，不因產業字元而失敗

### Requirement: generate_fonts online 模式動態抓取產業別
`generate_fonts.sh` 在 `--online` 模式 SHALL 嘗試從 TWSE 公司資料 API 下載產業別資料，提取 CJK 字元存入 `industry_symbols.txt`，並合併進最終字元集。

#### Scenario: online 模式產業 API 成功
- **WHEN** 以 `--online` 模式執行且 TWSE 產業 API 可達
- **THEN** `industry_symbols.txt` 有內容，最終字型包含所有回傳的產業名稱字元

#### Scenario: online 模式產業 API 失敗降級
- **WHEN** 以 `--online` 模式執行但 TWSE 產業 API 不可達或回傳非預期格式
- **THEN** 流程不中斷，跳過 `industry_symbols.txt`，仍使用靜態掃描的產業字元完成字型生成

#### Scenario: industry_symbols.txt 存在時納入合併
- **WHEN** `industry_symbols.txt` 存在且非空
- **THEN** 其字元與 `ui_symbols.txt`、`twse_symbols.txt` 合併去重，作為最終字元集

### Requirement: fetch_stock_chars 支援產業別模式
`fetch_stock_chars.py` SHALL 支援 `--mode industry` 參數，從 TWSE JSON 回應提取產業別名稱 CJK 字元。若欄位值為數字代碼，SHALL 查 `INDUSTRY_CODE_MAP` 轉換為中文名稱。若 API 無任何可辨識的產業資料，SHALL fallback 輸出全部 36 個已知產業名稱字元。

#### Scenario: 從產業欄位提取字元
- **WHEN** JSON 含 `產業別` 或 `業別` 欄位且值為中文名稱
- **THEN** 輸出這些名稱的去重 CJK 字元

#### Scenario: 產業代碼轉名稱
- **WHEN** JSON 的產業欄位值為數字代碼（如 `"24"`）
- **THEN** 查 `INDUSTRY_CODE_MAP` 轉換後（「半導體業」）提取 CJK 字元

#### Scenario: fallback 輸出已知產業字元
- **WHEN** JSON 無任何可辨識的產業欄位或欄位全為空值
- **THEN** 輸出 `INDUSTRY_CODE_MAP` 中全部 36 個產業名稱的 CJK 字元，確保不比靜態掃描差

#### Scenario: 預設模式向後相容
- **WHEN** 未傳 `--mode` 或傳 `--mode name`
- **THEN** 行為與原版相同，僅提取股票名稱字元
