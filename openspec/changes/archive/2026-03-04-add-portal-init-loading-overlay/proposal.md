## Why

Portal 初次載入時會先看到空白或局部 `Loading...` 文字，使用者無法判斷是正常等待、網路問題，或系統卡住。需要提供一致且可恢復的初始化狀態，降低誤判與重複操作。

## What Changes

- 在 Portal 首次載入加入全頁 loading 遮罩
- 將初始化請求（`/scan`、`/ai`、`/telegram`、`/saved_aps`、`/stocks`）納入單一 bootstrap gate
- 新增初始化逾時狀態：顯示全頁錯誤面板與重試按鈕
- 重試動作會重跑全部初始化請求，而非只重跑單一模組
- 不修改後端 API 路由與 JSON 回傳格式

## Capabilities

### New Capabilities
- （無）

### Modified Capabilities
- `portal-stock-management`: Portal 首次載入與逾時恢復流程新增全頁狀態管理與重試機制

## Impact

- `components/device_server/portal/index.html`：新增 overlay DOM/CSS 與初始化狀態機 JS
- Portal 使用者體驗：初始載入可視化、逾時可重試
- API 相容性：無 breaking change，沿用既有 endpoint
