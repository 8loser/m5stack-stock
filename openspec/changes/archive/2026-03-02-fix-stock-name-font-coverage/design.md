## Context

字型檔 `lv_font_noto_tc_14.c` / `lv_font_noto_tc_16.c` 由 `generate_fonts.sh` 呼叫 `lv_font_conv` 靜態生成，內嵌在韌體中。目前 `SYMBOLS` 字串只含 UI 標籤用漢字（~70字），未涵蓋 TWSE 股票名稱字元，導致 Dashboard 顯示亂碼。

字型生成是純開發機作業（host-side），不影響 Core2 runtime，但每次變更都需要重新 build + flash。

## Goals / Non-Goals

**Goals:**
- 自動從 TWSE 公開 API 取得股票名稱，擴充字型字符集
- 向下相容：網路不通或假日無資料時，fallback 原本 SYMBOLS，build 不中斷
- 不引入複雜 build-time 外部依賴（僅使用既有 `curl` + Python 標準庫）

**Non-Goals:**
- Runtime 動態字型載入（不在 Core2 上執行任何 Python）
- 涵蓋 ETF、認股權證等非一般股票名稱中的特殊字
- 自動偵測並重新生成字型（仍需開發者手動執行 `generate_fonts.sh`）

## Decisions

### D1：Shell 抓取 JSON，Python 專職解析與輸出 SYMBOLS

**決定**：`generate_fonts.sh` 先用 `curl` 下載 TWSE JSON，再呼叫 `fetch_stock_chars.py --input-json <file>` 解析並輸出 SYMBOLS；失敗時保留原始值。

**捨棄方案**：讓 Python script 直接連網抓 API — 會遇到執行環境 TLS/憑證差異，責任邊界不清晰。

### D2：使用 `curl` 抓檔，憑證失敗時自動 `curl -k` 重試

**決定**：使用 `curl` 下載 `https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL`。若正常模式失敗，再用 `curl -k` 重試；兩者都失敗才 fallback 預設 SYMBOLS。

### D3：CJK 字元範圍 U+4E00–U+9FFF

**決定**：只提取主要 CJK Unified Ideographs 區段，忽略 Extension A/B/C 等。台股名稱幾乎全在此範圍內，不需要更大的 block。

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| TWSE API 於假日回傳空陣列 | 解析腳本非零退出 → shell fallback 原始 SYMBOLS，build 仍可成功 |
| API 欄位名稱改版 | 解析腳本同時嘗試多個欄位名稱（如 `證券名稱` / `公司名稱` / `公司簡稱`） |
| 開發機憑證鏈差異導致 TLS 驗證失敗 | `curl` 失敗時自動 `curl -k` 重試；若仍失敗則 fallback |
| 字型體積增加 ~900KB | 16MB flash 可承受；`lv_font_conv --no-compress` 無壓縮，體積可預測 |
| 部分漢字在 Noto Sans TC 子集無 glyph | 極少見；lv_font_conv 遇到缺字會跳過（不報錯），顯示為空格 |

## Migration Plan

1. 新增 `fetch_stock_chars.py`
2. 修改 `generate_fonts.sh`，加入 `curl` 抓檔 + Python 解析邏輯
3. 在開發機執行 `./generate_fonts.sh`，確認輸出「使用擴充字符集」
4. 確認 `lv_font_noto_tc_14.c` / `lv_font_noto_tc_16.c` 體積增加
5. `./flash.sh --build-only` 確認編譯通過
6. `./flash.sh` 燒錄並驗證 Dashboard 股票名稱正確顯示

Rollback：git restore 兩個字型 .c 檔，重新 build + flash 即可回到原本版本。

## Open Questions

- 無（目前僅覆蓋 TWSE 上市名稱，符合現行需求）。
