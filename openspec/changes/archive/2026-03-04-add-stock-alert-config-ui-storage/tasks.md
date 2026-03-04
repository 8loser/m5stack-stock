## 1. Storage：新增 per-stock alert config 資料模型

- [x] 1.1 在 `components/storage/include/storage.h` 新增 `stock_alert_config_t` struct 與 save/load/remove API 宣告
- [x] 1.2 在 `components/storage/storage.c` 新增 alert config key helper（`a_<symbol>`）與 `storage_stock_alert_config_save()`
- [x] 1.3 實作 `storage_stock_alert_config_load()`：讀不到 key 時回傳預設值（enabled=false, thresholds=0, prompt=""）
- [x] 1.4 實作 `storage_stock_alert_config_remove()` 並處理 key 不存在時視為成功

## 2. Stock Admin API：擴充讀取與更新流程

- [x] 2.1 擴充 `GET /stocks` 回傳：每個 item 增加 `alert_config` 欄位
- [x] 2.2 新增 `POST /stocks/update` handler：解析 JSON 並驗證 symbol、threshold、prompt 長度
- [x] 2.3 新增錯誤碼處理（至少 `invalid_threshold`、`prompt_too_long`、`not_found`）
- [x] 2.4 在 `stock_admin_service_register_handlers()` 註冊 `/stocks/update` 路由
- [x] 2.5 在 `POST /stocks/add` 成功後建立預設 alert config
- [x] 2.6 在 `POST /stocks/remove` 成功後同步刪除 alert config

## 3. Portal 前端：Stocks 列表內展開編輯

- [x] 3.1 在 `components/device_server/portal/index.html` 新增每列 `Edit` 操作與展開編輯 UI
- [x] 3.2 新增欄位綁定：enabled checkbox、up/down threshold input、ai_prompt textarea
- [x] 3.3 實作儲存流程：呼叫 `POST /stocks/update`，成功後更新列表與摘要文案
- [x] 3.4 實作取消流程：放棄未儲存內容並恢復原值顯示
- [x] 3.5 補上提示文案：「單位時間依使用者設定的報價間隔」

## 4. 驗證

- [x] 4.1 `GET /stocks` 驗證：有無既有設定都能回傳完整 `alert_config`
- [x] 4.2 `POST /stocks/update` 成功路徑：更新後重載頁面與重開機都保留
- [x] 4.3 驗證失敗路徑：threshold 非法、prompt > 512、symbol 不存在時回應正確錯誤碼
- [x] 4.4 lifecycle 驗證：`/stocks/add` 建立預設設定、`/stocks/remove` 清除設定
- [x] 4.5 執行 `./flash.sh --build-only`，確保編譯通過
