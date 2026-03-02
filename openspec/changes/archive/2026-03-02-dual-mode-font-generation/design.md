## Context

目前 `tools/fonts/generate_fonts.sh` 依賴線上下載 TWSE JSON 才能產生字型，導致離線開發、CI 無網路或外部 API 暫時不可用時流程中斷。另一方面，團隊仍需要一個可主動更新股票名稱字集的模式，以便定期刷新 `twse_symbols.txt`。

## Goals / Non-Goals

**Goals:**
- 提供單一腳本的雙模式流程：`--online` 更新 TWSE 字集、`--offline` 使用既有字集。
- 預設為離線可重現流程，確保在無網路環境可穩定產生字型。
- 自動從 UI 原始碼萃取字元，降低手動維護 `ui_symbols.txt` 的成本與漏字風險。
- 維持現有 `lv_font_conv` 產物路徑與 14/16 字型輸出。

**Non-Goals:**
- 不改變韌體執行期字型載入方式或 UI API。
- 不新增第三方 Python 套件依賴。
- 不改動 `components/ui/fonts/*.c` 的格式選項（仍使用現行 `lv_font_conv` 參數）。

## Decisions

1. 使用單一入口腳本 `generate_fonts.sh`，以旗標切換模式。
- Rationale: 避免兩套腳本漂移；操作與 CI 都維持同一入口。
- Alternatives considered:
  - 兩支獨立腳本（online/offline）：可讀性高但重複邏輯高、易分叉。

2. 預設模式為 `--offline`。
- Rationale: 可重現、對網路零依賴，最符合日常開發穩定性。
- Alternatives considered:
  - 預設 online：可自動更新但脆弱，受外部 API 影響。

3. 新增 `extract_ui_symbols.py` 從 UI 原始碼產生 `ui_symbols.txt`。
- Rationale: 自動化維護 UI 字元來源，減少手動清單漏字。
- Alternatives considered:
  - 持續手動編輯 `ui_symbols.txt`：簡單但長期成本高且容易遺漏。

4. `--online` 下載失敗時直接失敗，不隱式 fallback。
- Rationale: 避免使用者誤判「已更新 TWSE 字集」。
- Alternatives considered:
  - 自動 fallback 離線：可提高成功率，但會模糊行為語意。

## Risks / Trade-offs

- [UI 掃描器可能漏掉非常態字串組裝] → 掃描器先涵蓋 C 字串常量與相鄰字串拼接，並保留 `ui_symbols.txt` 可人工補字。
- [offline 模式使用舊版 `twse_symbols.txt`] → 明確提供 `--online` 更新入口，並在文件標示更新時機。
- [雙模式增加腳本複雜度] → 集中參數解析與錯誤訊息，並以 tasks 納入模式分支測試。

## Migration Plan

1. 新增 `extract_ui_symbols.py`，先在本地驗證 `ui_symbols.txt` 產出。
2. 重構 `generate_fonts.sh`：加入 `--online/--offline` 參數與預設 offline。
3. 接線 online 模式至既有 `fetch_stock_chars.py` 下載解析流程。
4. 接線 offline 模式檢查既有 `twse_symbols.txt`。
5. 驗證兩模式都可產生 `lv_font_noto_tc_14.c`、`lv_font_noto_tc_16.c`。

## Open Questions

- 無。`twse_symbols.txt` 更新策略由 `--online` 明確觸發，預設離線已定案。
