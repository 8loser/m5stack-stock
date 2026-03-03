## Context

字型生成工具 (`tools/fonts/`) 目前有三個檔案：
- `generate_fonts.sh`：主流程，offline/online 兩種模式
- `extract_ui_symbols.py`：掃描 `components/ui/` 取非 ASCII 字元 → `ui_symbols.txt`
- `fetch_stock_chars.py`：解析 TWSE JSON 取股票名稱 CJK 字元 → `twse_symbols.txt`

`s_industry_map`（36 個產業名稱）在 `components/twse_client/twse_client.c`，目前完全沒有被任何工具掃描。Dashboard 的產業別欄位使用 `lv_font_noto_tc_14`，若字型缺少「導」「電」「組」等字，會顯示方塊。

## Goals / Non-Goals

**Goals:**
- 確保所有 36 個 hardcoded 產業名稱的 CJK 字元納入字型（兩種模式均有效）
- online 模式額外從 TWSE API 動態抓取產業別，補充 hardcoded map 未涵蓋的名稱
- `extract_ui_symbols.py` 可重複使用於任意 C 原始碼目錄（不限 `components/ui/`）

**Non-Goals:**
- 不修改 ESP32 韌體程式碼
- 不改變字型格式或 bpp 設定
- `industry_symbols.txt` 不需納入版本控制

## Decisions

**D1：`extract_ui_symbols.py --src` 改為 `action="append"`**

允許多次傳入 `--src <dir>`，`collect_symbols` 接受 `list[Path]` 並 union 結果。預設行為保持不變（預設仍 `components/ui`）。

備選：直接改 `generate_fonts.sh` 呼叫兩次並 merge 輸出。缺點：產生兩個暫存檔，邏輯分散。

**D2：掃描 `components/twse_client/` 納入靜態產業字元（兩種模式）**

`s_industry_map` 的 36 個名稱是 C 字串字面值，`extract_ui_symbols.py` 已能正確解析。只需在 `generate_fonts.sh` 多傳一個 `--src` 即可，零額外工具。

備選：維護獨立 `industry_names.txt`。缺點：與 `twse_client.c` 脫鉤，需手動同步。

**D3：online 模式新增 TWSE 公司資料 API（動態產業別）**

新增 `TWSE_INDUSTRY_URL`（`getStockInfo` 或 `t187ap03_L`，現場驗證），下載後以 `fetch_stock_chars.py --mode industry` 提取產業名稱 CJK 字元。

`fetch_stock_chars.py` 新增：
- `TWSE_INDUSTRY_KEYS = ("產業別", "業別", "industry", "Industry")`
- `INDUSTRY_CODE_MAP`（Python 版 `s_industry_map`）作為 fallback：若 API 欄位為數字代碼則查表，若無任何資料則直接輸出所有已知產業名稱
- `--mode {name,industry}` 參數（預設 `name`，向後相容）

**D4：industry fetch 失敗不中斷流程**

online 模式 industry fetch 以 `|| true` 處理失敗，`industry_symbols.txt` 缺失時 merge 步驟直接跳過。靜態掃描（D2）已確保基本覆蓋，不需強制 API 成功。

## Risks / Trade-offs

- [風險] `TWSE_INDUSTRY_URL` 尚未驗證格式與欄位名 → 以 fallback（輸出全 36 個已知名稱）確保 worst case 不比現在差
- [風險] `INDUSTRY_CODE_MAP` 與 `s_industry_map` 雙份維護 → 在 Python 程式碼旁加 comment 標明需與 C 檔同步
- [Trade-off] 掃描 `components/twse_client/` 會納入該目錄所有 C 字串，包含 log 訊息的中文字元 → 可接受，字型多一點字不影響功能，只略增 `.c` 檔大小

## Open Questions

- `TWSE_INDUSTRY_URL` 確切 endpoint 需現場 curl 驗證（`getStockInfo` vs `t187ap03_L`）；實作時先試 `getStockInfo`，失敗改 `t187ap03_L`
