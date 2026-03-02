## Why

目前使用者在 Portal 新增股票時已能取得產業別，但 Dashboard 只顯示代號、名稱、價格與漲跌幅，缺少產業脈絡。這使多檔輪播時辨識效率較差，且 Portal 與裝置端資訊不一致。

## What Changes

- 擴充股票 metadata：新增 `industry` 欄位，並在新增股票成功後一併保存（RAM cache + NVS）。
- 調整 `GET /stocks` 與 `POST /stocks/add` 回傳：每筆股票資料包含 `industry`。
- 擴充 `stock_quote_t` 或等效 UI 輸入資料，讓 Dashboard 更新時可直接取得 `industry`。
- Dashboard 版面由 3 欄調整為 4 欄：`symbol+name | industry | price | change%`。
- 保持既有策略：產業別查詢失敗不阻擋新增，回傳空字串或未知值。

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `portal-stock-management`: `/stocks` 與 `/stocks/add` 的資料契約新增 `industry`，並要求 metadata 持久化與刪除同步。
- `twse-client`: 股票驗證/metadata 查詢流程需支援 `industry` 解析與輸出。
- `dashboard-display`: Dashboard 卡片資訊模型與版面需求改為四欄，新增產業欄顯示規範。

## Impact

- Affected code:
  - `components/twse_client/include/twse_client.h`
  - `components/twse_client/include/twse_models.h`
  - `components/twse_client/twse_client.c`
  - `components/device_server/device_server.c`
  - `components/storage/include/storage.h`
  - `components/storage/storage.c`
  - `components/ui/screens/screen_dashboard.c`
  - `components/ui/widgets/stock_card.c`（若需保持 widget 一致）
- API impact: Portal 股票管理 API JSON 將新增 `industry` 欄位（向後相容的欄位擴充）。
- Data impact: NVS 新增股票 metadata 儲存鍵值（name/abbr/industry）。
- Runtime impact: 新增股票時多一次 metadata enrich；一般 quote/排程路徑不主動查產業別。
