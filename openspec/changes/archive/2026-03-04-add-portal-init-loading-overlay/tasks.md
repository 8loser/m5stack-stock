## 1. Portal Overlay UI

- [x] 1.1 在 `components/device_server/portal/index.html` 新增全頁 overlay DOM（loading、timeout_error、hidden）
- [x] 1.2 新增 overlay CSS（全頁覆蓋、層級、錯誤面板、Retry 按鈕）

## 2. 初始化狀態機

- [x] 2.1 新增 bootstrap 入口函式，集中管理首次載入初始化任務
- [x] 2.2 將 `/scan`、`/ai`、`loadTelegram()`、`loadSavedAps()`、`loadStocks()` 納入同一 init task wrapper
- [x] 2.3 新增 timeout 控制（預設 10000ms），逾時切換至 timeout_error
- [x] 2.4 新增 Retry 行為，按下後重跑全部初始化任務
- [x] 2.5 加入 generation/race 保護，避免舊請求覆蓋新輪次狀態

## 3. 相容性與回歸

- [x] 3.1 確認既有分頁切換（WiFi/AI/Telegram/Stocks）不被 overlay 狀態機破壞
- [x] 3.2 確認各區塊原有錯誤訊息仍可正常顯示（如 `Load failed`）
- [x] 3.3 手動驗證正常、逾時、重試成功、連續重試四種情境
- [x] 3.4 執行 `./flash.sh --build-only` 確認編譯通過
