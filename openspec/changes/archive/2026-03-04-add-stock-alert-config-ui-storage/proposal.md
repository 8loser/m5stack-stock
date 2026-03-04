## Why

目前 Portal 的股票清單只支援新增/刪除，無法為單一股票設定告警門檻與對應 AI prompt，導致後續規則告警功能缺少可配置資料來源。需要先完成 Web 介面與持久化儲存，才能在下一階段安全接上觸發與通知機制。

## What Changes

- 擴充 `GET /stocks` 回傳內容，新增每檔股票 `alert_config`（enabled、up/down threshold、ai_prompt）
- 新增 `POST /stocks/update`，支援單檔股票更新告警設定
- 擴充 Stocks 頁面為「列表內展開編輯」模式，可編輯啟用開關、上下行門檻、個股 prompt
- 新增 NVS 儲存模型與 API：每檔股票獨立儲存 alert config
- `POST /stocks/add` 成功後建立預設 alert config（預設關閉）；`POST /stocks/remove` 同步刪除 alert config
- 本變更不實作告警觸發、AI 呼叫 API、Telegram 推播

## Capabilities

### New Capabilities

- （無）

### Modified Capabilities

- `portal-stock-management`: 擴充股票管理能力，新增 per-stock alert 設定編輯與儲存 API

## Impact

- `components/storage/include/storage.h`：新增 `stock_alert_config_t` 與 save/load/remove API 宣告
- `components/storage/storage.c`：新增 per-stock alert config 的 NVS key 與持久化實作
- `components/device_server/stock_admin_service.c`：擴充 `GET /stocks`、新增 `POST /stocks/update`、整合 add/remove lifecycle
- `components/device_server/portal/index.html`：Stocks tab 新增 inline 編輯介面與儲存流程
- API 變更：`GET /stocks` response schema 新增欄位；新增 `POST /stocks/update`
