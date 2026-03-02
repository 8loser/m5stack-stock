## ADDED Requirements

### Requirement: UI 字元掃描腳本產生 ui_symbols.txt
專案 SHALL 包含 `tools/fonts/extract_ui_symbols.py`。該腳本 SHALL 掃描 UI 原始碼字串常量，抽取 UI 所需中文字元與常用符號，去重後輸出到 `tools/fonts/ui_symbols.txt`，供字型產生流程使用。

#### Scenario: 成功掃描 UI 原始碼
- **WHEN** 執行 UI 字元掃描腳本，且輸入目錄存在可解析字串
- **THEN** 產生非空且去重後的 `ui_symbols.txt`

#### Scenario: 輸入來源無法掃描
- **WHEN** 執行 UI 字元掃描腳本，且輸入目錄不存在或無法讀取
- **THEN** 腳本以非零狀態碼退出並輸出錯誤訊息

## MODIFIED Requirements

### Requirement: 字型產生腳本
專案 SHALL 包含字型產生腳本（`generate_fonts.sh`）。腳本 SHALL 先執行 UI 字元掃描流程產生 `ui_symbols.txt`，再依模式處理 TWSE 字元來源：`--online` 模式 SHALL 下載 TWSE JSON 並更新 `twse_symbols.txt`；`--offline` 模式 SHALL 直接使用既有 `twse_symbols.txt`。腳本在未指定模式時 SHALL 預設使用 `--offline`。最終 SHALL 合併 `ui_symbols.txt` 與 `twse_symbols.txt` 作為 `lv_font_conv --symbols` 輸入，並產生 14/16 字型。

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
