## Context

目前系統以 `MAX_STOCK_COUNT=10` 為核心，但實作中仍存在多處重複容量定義（例如固定陣列與模組內常數）。若只改單一位置，會出現 add API、storage、UI cache、scheduler buffer、TWSE client 之間的容量不一致，造成越界或邏輯錯誤。

本次需求已明確接受「股票數增加後輪播變慢」，因此設計重點是容量一致性與穩定性，而非顯示節奏優化。

## Goals / Non-Goals

**Goals:**
- 將股票上限安全提升到 15。
- 所有容量相關模組使用同一來源常數，避免硬編碼分岔。
- 保持 API schema 與錯誤碼不變，只調整上限行為與文案。
- 同步 OpenSpec 規格，避免程式與規格不一致。

**Non-Goals:**
- 不新增 runtime 可配置上限。
- 不改變 dashboard 5 列固定格位與輪播策略。
- 不在本次引入 TWSE 分批請求或效能優化。

## Decisions

1. 採用 compile-time 固定上限 15
- Decision: `MAX_STOCK_COUNT` 直接由 10 改為 15。
- Rationale: 需求明確、改動最可控，且不增加設定面複雜度。
- Alternative: runtime 設定上限（NVS 或 Kconfig）。
  - Rejected: 需新增驗證與相容處理，超出本次目的。

2. 容量單一來源化
- Decision: 讓 `stock_list_t`、TWSE client 內部列表容量、queue 深度與 UI/scheduler 快取均以 `MAX_STOCK_COUNT` 對齊。
- Rationale: 防止局部調整造成越界或 silently truncate。
- Alternative: 保留模組內獨立常數（如 `MAX_SYMBOLS`）。
  - Rejected: 日後調整上限仍高機率遺漏。

3. 維持既有 API 契約
- Decision: `limit_exceeded`、HTTP status 與 JSON schema 不變。
- Rationale: 降低前端與整合端回歸成本。
- Alternative: 新增錯誤碼或回傳更多容量資訊。
  - Rejected: 非必要，且屬行為外擴。

4. 規格與文案同次更新
- Decision: 同步更新 OpenSpec 現行 spec 與 Portal 文案中的上限描述。
- Rationale: 確保需求、實作、對外顯示一致。
- Alternative: 僅改程式碼。
  - Rejected: 會留下文件偏差，增加後續維護成本。

## Risks / Trade-offs

- [Risk] TWSE 單次回應與 JSON 解析負載增加，可能提高 timeout 機率。
  - Mitigation: 先保持現行通訊策略，透過 monitor 驗證失敗率；如有需要另開變更做分批抓取。

- [Trade-off] 股票數提高會延長單檔輪播覆蓋週期。
  - Mitigation: 本次明確接受；不納入優化範圍。

- [Risk] 若仍有遺漏硬編碼容量，會造成不一致。
  - Mitigation: 以 `MAX_STOCK_COUNT` 全域搜尋與逐點對齊，並以 build + 手測驗證。

## Migration Plan

1. 調整核心常數至 15。
2. 對齊 storage 型別與 TWSE client 內部容量定義。
3. 同步 portal API 行為文案與 UI 提示文案。
4. 更新 OpenSpec delta specs（portal/twse/dashboard）。
5. 執行 `./flash.sh --build-only` 與實機手測（1~15 成功、16 拒絕）。

## Open Questions

- None.
