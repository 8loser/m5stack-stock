## Context

目前 `device_server` 透過 `twse_client_validate_symbol()` 驗證股票代號，並把 `name/abbr` 暫存在 `s_stock_meta_cache`。Dashboard 更新路徑則只接收 `stock_quote_t`，未包含 `industry`，因此 UI 無法顯示產業欄。

系統約束如下：
- Dashboard 只能在 LVGL thread-safe 規則下更新（需透過既有 UI 更新路徑）。
- 使用者已要求「產業別只在新增股票時抓取」，不可在 quote 週期或 dashboard render 時額外打 API。
- 現有 `stock_list_t` 僅保存 symbol，metadata 需另存。

## Goals / Non-Goals

**Goals:**
- 新增股票成功時抓取並保存 `industry`。
- Dashboard 顯示四欄，第二欄固定顯示 `industry`。
- 重開機後仍能顯示先前已抓到的 `industry`。
- 產業別抓取失敗不阻擋新增流程。

**Non-Goals:**
- 不改變 quote 抓取頻率與 scheduler 行為。
- 不在 `GET /stocks`、Dashboard 刷新、或排程抓價時補抓 `industry`。
- 不在本次變更重構 AI 分析資料模型。

## Decisions

1. 以「新增時 enrich」作為唯一產業來源
- Decision: 僅在 `POST /stocks/add` 驗證成功後抓取 `industry`。
- Rationale: 符合需求、可控網路負擔、避免 dashboard 路徑阻塞。
- Alternative: 在 dashboard 顯示時按 symbol lazy-load。
  - Rejected: 會引入 UI-網路耦合與延遲，且違反「只在新增時抓」。

2. 將 metadata 持久化到 NVS
- Decision: 新增 symbol metadata 儲存 API（save/load/remove），保存 `name/abbr/industry`。
- Rationale: 重開機後不丟失，避免再次查詢。
- Alternative: 僅 RAM cache。
  - Rejected: 重啟即遺失，顯示不穩定。

3. Dashboard 由 quote 輸入攜帶 industry
- Decision: 擴充 `stock_quote_t`（或等價 UI DTO）加入 `industry`，UI 不做查詢。
- Rationale: 單向資料流清晰，UI 保持被動渲染。
- Alternative: Dashboard 以 symbol 向 storage 查 metadata。
  - Rejected: 增加 UI 與 storage 耦合，並需額外同步策略。

4. 四欄版面固定為 `symbol+name | industry | price | change%`
- Decision: 左欄保留兩行（symbol+name），第二欄新增 industry 單行，右兩欄維持價格與漲跌。
- Rationale: 資訊完整且可讀性最佳，符合使用者指定。
- Alternative: 僅 symbol + industry，不顯示 name。
  - Rejected: 失去公司識別資訊，不利快速辨識。

## Risks / Trade-offs

- [Risk] 產業字串較長導致卡片擁擠或截斷。
  - Mitigation: 第二欄採固定寬度與省略策略（ellipsis），避免擠壓 price/change 欄。

- [Risk] 既有資料（歷史股票）沒有 `industry`。
  - Mitigation: 回傳空字串或「未知」，不自動補抓。

- [Risk] metadata 寫入與刪除流程遺漏，造成殘值。
  - Mitigation: 在 `/stocks/add` 與 `/stocks/remove` 成功路徑加明確保存/刪除步驟與測試案例。

- [Trade-off] 不在列表載入時補抓，會讓舊資料長期顯示未知。
  - Mitigation: 保持需求邊界；若要補齊另開 change。

## Migration Plan

1. 新增 metadata 結構與 storage API（含 NVS key 規範）。
2. 擴充 twse client metadata 解析，提供 `industry`。
3. 串接 `POST /stocks/add` enrich + persist 與 `POST /stocks/remove` metadata cleanup。
4. 擴充 quote/UI 資料流攜帶 `industry`。
5. 調整 Dashboard 四欄 layout 與字串截斷策略。
6. 進行 build 與手動驗證（新增、刪除、重開機、顯示回歸）。

## Open Questions

- TWSE 目前使用的驗證端點是否穩定提供產業欄位；若無，需固定替代資料源與解析對照規則。
