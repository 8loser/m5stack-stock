## MODIFIED Requirements

### Requirement: twse_client_validate_symbol 驗證代號合法性
`twse_client_validate_symbol(symbol, out)` SHALL 呼叫 TWSE stock info API（`ex_ch=tse_<symbol>.tw`），解析 `msgArray[0]` 回傳 `stock_symbol_info_t`：`symbol`、`name`、`short_name`、`market`、`industry`、`exists`。其中 `industry` SHALL 由同次查詢可得欄位或既定 metadata 來源填入；若產業別不可得，SHALL 設為空字串且不得影響 `exists`/`market` 判定。若 `msgArray` 為空或解析失敗，SHALL 設 `exists=false` 並回傳 `ESP_OK`（代號不存在，非 API 錯誤）。HTTP 失敗或 JSON 解析失敗才回傳 `ESP_FAIL`。

#### Scenario: 合法上市股票且含產業別
- **WHEN** 傳入有效上市代號（如 "2330"）且 metadata 可解析
- **THEN** `out->exists=true`、`out->market="tse"`、`out->name` 與 `out->short_name` 非空、`out->industry` 非空，回傳 `ESP_OK`

#### Scenario: 合法上市股票但無產業資料
- **WHEN** 傳入有效上市代號但來源未提供產業欄位
- **THEN** `out->exists=true`、`out->market="tse"`、`out->industry` 為空字串，回傳 `ESP_OK`

#### Scenario: 不存在代號
- **WHEN** 傳入不存在代號（如 "9999"）
- **THEN** `out->exists=false`，回傳 `ESP_OK`

#### Scenario: 上櫃（OTC）股票
- **WHEN** 傳入上櫃代號（如 "6547"）
- **THEN** `out->exists=true`、`out->market` 非 "tse"，回傳 `ESP_OK`（由呼叫方判斷 market）

#### Scenario: 網路不通
- **WHEN** HTTP 請求失敗（逾時或無網路）
- **THEN** 回傳 `ESP_FAIL`
