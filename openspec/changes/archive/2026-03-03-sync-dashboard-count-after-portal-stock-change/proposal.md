## Why

目前在 Portal 網頁新增或刪除股票後，NVS 與 scheduler 會更新，但 dashboard 的卡片數不會立即同步，使用者切回 dashboard 會看到舊數量，直到下一次報價更新才可能反映。這造成操作成功但畫面延遲一致性的體驗問題。

## What Changes

- 在 `device_server` 增加「股票清單變更」callback 介面，於 `/stocks/add`、`/stocks/remove` 成功後回報最新 count。
- 在 `main` 註冊 callback，橋接到 `ui_manager_set_dashboard_card_count()`，讓 UI 立即同步卡片數。
- 明確定義失敗路徑（驗證失敗/儲存失敗）不得更新 dashboard count。
- 維持既有行為：仍由 scheduler 在排程週期更新報價，不新增立即抓價。

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `portal-stock-management`: 調整 `/stocks/add` 與 `/stocks/remove` 成功後的系統行為，要求同步通知 dashboard 卡片數。
- `dashboard-display`: 擴充動態卡片數需求，要求在 Portal 成功新增/刪除股票後，切回 dashboard 時立即反映新 count，不需等待下一筆 quote。

## Impact

- Affected code:
  - `components/device_server/include/device_server.h`
  - `components/device_server/device_server.c`
  - `main/main.c`
- Public interface impact: `device_server` 將新增一個 callback 註冊 API（不影響既有 API 相容性）。
- Runtime impact: 無新增外部依賴；僅在股票清單修改成功時多一次回呼。
