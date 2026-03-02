## 1. UI 字元掃描器

- [x] 1.1 新增 `tools/fonts/extract_ui_symbols.py`，掃描 `components/ui` 字串常量並輸出去重後字元集合。
- [x] 1.2 支援相鄰字串拼接解析與錯誤處理（來源目錄不存在/不可讀時非零退出）。
- [x] 1.3 將輸出寫入 `tools/fonts/ui_symbols.txt`，確保輸出順序穩定。

## 2. generate_fonts.sh 雙模式

- [x] 2.1 重構 `tools/fonts/generate_fonts.sh` 參數解析，加入 `--online` 與 `--offline`，未指定時預設 `--offline`。
- [x] 2.2 在腳本起始階段接線 `extract_ui_symbols.py`，每次先更新 `ui_symbols.txt`。
- [x] 2.3 實作 `--online`：下載 TWSE JSON，呼叫 `fetch_stock_chars.py` 更新 `twse_symbols.txt`。
- [x] 2.4 實作 `--offline`：驗證既有 `twse_symbols.txt` 存在且非空，否則失敗退出。
- [x] 2.5 合併 `ui_symbols.txt` + `twse_symbols.txt` 作為 `lv_font_conv --symbols` 輸入，維持 14/16 字型輸出。

## 3. 驗證與文件

- [x] 3.1 驗證 `./tools/fonts/generate_fonts.sh --offline` 在無網路環境可成功產生字型。
- [x] 3.2 驗證 `./tools/fonts/generate_fonts.sh --online` 可更新 `twse_symbols.txt` 並產生字型。（以 mock curl 注入本地 TWSE JSON 驗證流程）
- [x] 3.3 驗證 `twse_symbols.txt` 缺失或空檔時 `--offline` 正確失敗且不執行 `lv_font_conv`。
- [x] 3.4 更新相關開發說明，記錄雙模式用法與預設模式為 offline。
