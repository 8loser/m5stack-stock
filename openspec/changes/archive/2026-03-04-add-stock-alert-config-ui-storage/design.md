## Context

目前 `portal-stock-management` 已有 `GET /stocks`、`POST /stocks/add`、`POST /stocks/remove` 與前端列表呈現，但沒有 per-stock 規則參數。此變更屬跨模組擴充（portal UI、HTTP handlers、storage），且新增資料模型（NVS 結構），需要先明確資料格式與相容策略，避免後續接入告警觸發時出現資料不一致。

## Goals / Non-Goals

**Goals:**
- 新增 per-stock `alert_config` 的資料模型與持久化 API
- 提供單檔更新 API（`POST /stocks/update`）並完成驗證規則
- 擴充 Stocks tab 為列表內展開編輯，支援保存與取消
- 確保舊資料可無縫升級（缺少 alert_config 時回預設值）

**Non-Goals:**
- 不實作門檻判斷觸發流程
- 不實作 AI API 呼叫與通知機制
- 不改動 scheduler 的抓價節奏與策略

## Decisions

1. 使用每檔獨立 blob key 儲存 alert config（`a_<symbol>`）
- Why: 與既有 meta key（`m_<symbol>`）模式一致，便於 add/remove 同步管理。
- Alternative: 以單一大 blob 儲存全清單設定；缺點是單檔更新需要整包覆寫，碰撞風險較高。

2. 預設設定採安全模式（`enabled=false`）
- Why: 升級後不會意外啟用告警，符合「先配置再啟用」流程。
- Alternative: 預設啟用或依門檻自動啟用；缺點是容易在舊資料升級時產生非預期行為。

3. API 採 `POST /stocks/update` 而非 RESTful path
- Why: 與現有 `/stocks/add`、`/stocks/remove` 風格一致，降低改動範圍與心智負擔。
- Alternative: `PUT /stocks/{symbol}`；缺點是現有 router 設計沒有 path param 處理慣例。

4. Prompt 長度限制 512 bytes（UTF-8）
- Why: 控制 NVS/記憶體負擔，足夠 v1 告警敘述。
- Alternative: 1024 bytes；彈性更大但嵌入式環境成本較高。

5. threshold 採雙向獨立數值（上漲/下跌）
- Why: 可分別控制上行與下行敏感度，後續規則引擎可直接使用。
- Alternative: 單一絕對值；配置較簡單但表達力不足。

## Risks / Trade-offs

- [NVS key 數量增加] → 維持每檔 1 key，上限 15 檔，空間可控；刪除股票時同步刪 key
- [浮點驗證邊界] → 明確限制 0..99.99 並檢查 finite，拒絕 NaN/Inf
- [前端 inline 編輯狀態複雜] → 單列編輯狀態獨立管理，Save/Cancel 都回到單一渲染入口
- [舊資料相容] → load 不到 key 時回預設值，不要求 migration 一次性搬遷

## Migration Plan

1. 先上 storage API 與預設回退邏輯（不改現有讀取流程）
2. 擴充 `/stocks` 回傳欄位與 `/stocks/update` handler
3. 更新 portal 前端 Stocks tab，接上編輯與儲存
4. 驗證 add/remove lifecycle 與重啟持久化
5. 若需 rollback，可先移除前端編輯入口與 `/stocks/update` 註冊；既有清單 API 仍可維持運作

## Open Questions

- 無（本次範圍、欄位、限制、預設值已定案）
