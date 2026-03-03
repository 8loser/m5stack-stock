# twse-client Specification

## Purpose
TBD - created by archiving change twse-portal-stock-management. Update Purpose after archive.
## Requirements
### Requirement: twse_client_fetch 容量需與全域股票上限一致
`twse_client_fetch` 與其內部工作緩衝 SHALL 以 `MAX_STOCK_COUNT` 作為股票列表容量上限來源，不得使用獨立硬編碼上限，避免與其他模組產生容量不一致。

#### Scenario: 以 15 檔設定編譯
- **WHEN** 專案將 `MAX_STOCK_COUNT` 設為 15
- **THEN** TWSE client 的 symbol 容量與結果緩衝 SHALL 可正確容納 15 檔且不需額外改碼

#### Scenario: 調整全域上限時自動對齊
- **WHEN** `MAX_STOCK_COUNT` 在未來被調整
- **THEN** TWSE client 容量 SHALL 自動對齊新值，不得保留舊硬編碼上限

### Requirement: twse_client_validate_symbol 驗證代號合法性
`twse_client_validate_symbol(symbol, out)` SHALL 呼叫 TWSE stock info API（`ex_ch=tse_<symbol>.tw`），解析 `msgArray[0]` 回傳 `stock_symbol_info_t`：`symbol`、`name`（短名）、`short_name`（英文）、`industry`、`market`（"tse" 或其他）、`exists`（bool）。若 `msgArray` 為空或解析失敗，SHALL 設 `exists=false` 並回傳 `ESP_OK`（代號不存在，非 API 錯誤）。HTTP 失敗或 JSON 解析失敗才回傳 `ESP_FAIL`。

#### Scenario: 合法上市股票
- **WHEN** 傳入有效上市代號（如 "2330"）
- **THEN** `out->exists=true`、`out->market="tse"`、`out->name` 與 `out->short_name` 非空，回傳 `ESP_OK`

#### Scenario: 不存在代號
- **WHEN** 傳入不存在代號（如 "9999"）
- **THEN** `out->exists=false`，回傳 `ESP_OK`

#### Scenario: 上櫃（OTC）股票
- **WHEN** 傳入上櫃代號（如 "6547"）
- **THEN** `out->exists=true`、`out->market` 非 "tse"，回傳 `ESP_OK`（由呼叫方判斷 market）

#### Scenario: 網路不通
- **WHEN** HTTP 請求失敗（逾時或無網路）
- **THEN** 回傳 `ESP_FAIL`

#### Scenario: 產業欄位缺值
- **WHEN** TWSE 回應未提供可用產業欄位
- **THEN** `out->industry` SHALL 為空字串，且不影響 `exists` 與 `market` 判定
