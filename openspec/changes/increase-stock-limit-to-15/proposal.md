## Why

目前股票監測上限為 10，且容量定義分散在多個模組（含硬編碼），提高上限時容易遺漏並導致容量不一致風險。需求已確認可接受輪播變慢，因此需要把上限安全提升到 15，並同步規格與文案。

## What Changes

- 將股票監測上限由 10 提升至 15（`MAX_STOCK_COUNT`）。
- 移除重複硬編碼容量，改為以單一容量來源對齊 storage、TWSE client、scheduler、UI、queue。
- 維持既有 API contract（路徑、欄位與錯誤碼不變），僅調整上限行為。
- 更新 Portal 文案（`max 10`/`最多 10 檔` -> `15`）。
- 同步更新現行 OpenSpec 規格中的 10 檔限制描述為 15。

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `portal-stock-management`: `POST /stocks/add` 上限由 10 調整為 15，相關情境與文案同步更新。
- `dashboard-display`: 動態股票數量需求中的上限由 10 調整為 15。
- `twse-client`: 股票列表處理容量需求需與 `MAX_STOCK_COUNT` 對齊，避免模組間容量不一致。

## Impact

- Affected code:
  - `include/app_config.h`
  - `components/storage/include/storage.h`
  - `components/storage/storage.c`
  - `components/twse_client/twse_client.c`
  - `components/device_server/device_server.c`
  - `components/scheduler/scheduler.c`
  - `components/ui/screens/screen_dashboard.c`
  - `main/main.c`
- API impact: `POST /stocks/add` 限制行為改為 15 檔；error code 與 schema 不變。
- Data impact: `stock_list_t` 容量隨常數放大，NVS 讀寫流程維持相容。
- Runtime impact: 股票數增加會拉長輪播覆蓋週期（已接受）。
