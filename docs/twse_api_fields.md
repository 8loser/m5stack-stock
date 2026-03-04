# TWSE API 回傳欄位對照（本專案）

本專案使用 TWSE MIS API：

`https://mis.twse.com.tw/stock/api/getStockInfo.jsp?ex_ch=tse_<symbol>.tw|...&json=1&delay=0`

主要資料位於 `msgArray`（每檔股票一個 object）。

## 欄位對照

| 欄位 | 說明 | 本專案用途 |
|---|---|---|
| `c` | 股票代號（例如 `2330`） | 主鍵對位 |
| `ch` | 頻道字串（例如 `tse_2330.tw`） | `c` 缺值時由 `ch` 回推出 symbol |
| `n` | 股票簡稱 | 顯示名稱、symbol metadata 的 short_name |
| `nf` | 股票全名 | symbol metadata 的 name 優先來源 |
| `z` | 最新成交價 | 現價首選 |
| `o` | 開盤價 | 報價欄位 |
| `h` | 最高價 | 報價欄位 |
| `l` | 最低價 | 報價欄位 |
| `y` | 昨收價 | 漲跌計算、休市顯示基準 |
| `v` | 成交量 | 零量快照判斷 |
| `t` | 成交時間 | 顯示交易時間 |
| `u` | 漲停價 | 漲跌停判斷 |
| `w` | 跌停價 | 漲跌停判斷 |
| `a` | 最佳賣價字串（`_` 分隔） | `z` 不可用時備援現價（取第一檔） |
| `b` | 最佳買價字串（`_` 分隔） | `z` 不可用時備援現價（取第一檔） |
| `ex` | 市場別（例如 `tse`） | `validate_symbol` 判斷是否上市 |
| `industry` | 產業文字 | symbol metadata 產業優先來源 |
| `i` | 產業代碼或文字 | `industry` 缺值時備援（代碼映射或直接使用文字） |

## 解析規則（目前程式行為）

- `"-"` 視為該欄位無資料。
- 現價來源優先序：`z` -> `b` 第一檔 -> `a` 第一檔 -> `y`。
- 休市快照判定：
  - `v <= 0`
  - 且 `z/o/h/l/a/b` 皆不可用
  - 且 `y` 可用
  - 以上成立時，標記 `is_market_closed = true`。
- 有 `z` 或可用 `a/b` 任一者時，標記 `is_valid = true`。

## 對應程式位置

- 報價解析：`components/twse_client/twse_client.c` 的 `parse_stock_item()`
- 代號驗證與 metadata：`components/twse_client/twse_client.c` 的 `twse_client_validate_symbol()` / `parse_symbol_metadata()`
- 資料模型：`components/twse_client/include/twse_models.h`、`components/twse_client/include/twse_client.h`
