## 1. Capacity Alignment

- [x] 1.1 將 `MAX_STOCK_COUNT` 由 10 提升到 15（`include/app_config.h`）
- [x] 1.2 將 `stock_list_t.symbols[10][8]` 改為 `symbols[MAX_STOCK_COUNT][8]`（`components/storage/include/storage.h`）
- [x] 1.3 對齊 `twse_client` 內部 symbol 容量常數至 `MAX_STOCK_COUNT`，移除重複硬編碼上限
- [x] 1.4 檢查並確認 scheduler/UI/device_server/main queue 的容量使用皆一致對齊 `MAX_STOCK_COUNT`

## 2. Portal Behavior and Text Sync

- [x] 2.1 保持 `POST /stocks/add` 的錯誤碼與 schema 不變，僅將上限行為調整為 15
- [x] 2.2 更新 Portal Stocks 標題文案 `max 10` 為 `max 15`
- [x] 2.3 更新前端 `limit_exceeded` 對應訊息為「最多 15 檔」

## 3. OpenSpec Spec Sync

- [x] 3.1 更新 `portal-stock-management` delta：上限與超限 scenario 由 10 改為 15
- [x] 3.2 更新 `dashboard-display` delta：固定格位輪巡上限由 10 改為 15
- [x] 3.3 更新 `twse-client` delta：容量要求與 `MAX_STOCK_COUNT` 對齊

## 4. Validation

- [x] 4.1 執行 `./flash.sh --build-only` 並確認編譯通過
- [x] 4.2 手動驗證可新增 1~15 檔，新增第 16 檔回 `409 limit_exceeded`
- [x] 4.3 手動驗證刪除後可再次新增至 15，且 dashboard count 同步
- [x] 4.4 執行 `./flash.sh --monitor` 觀察 log，確認無越界/重啟異常
