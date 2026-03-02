## Context

現況在 `screen_dashboard.c` 使用 page flip timer 依 `DASHBOARD_PAGE_FLIP_S` 做整頁切換。此作法在股票數量 >5 時會一次改變整批內容，雖然實作簡單，但使用者閱讀追蹤成本高。現有報價更新由 scheduler queue 驅動，UI 已有快取機制可支援解耦刷新。

## Goals / Non-Goals

**Goals:**
- Dashboard 固定 5 格位置，不再做整頁翻頁。
- 股票數量 >5 時以固定節拍（2 秒）每次替換 1 格內容。
- 輪巡順序依設定清單順序，並可循環。
- card 顏色（漲跌色）始終對應該格當前顯示之股票資料。
- 回到 Dashboard 時重置輪巡起點。

**Non-Goals:**
- 不改變 card 版面尺寸與字體配置。
- 不在本次加入設定頁可調 UI 輪巡秒數。
- 不改 web portal 股票管理行為。

## Decisions

1. 以「slot mapping」取代「page index」。
- 決策: 5 個可視格位各自綁定目前顯示的 stock index，另維護 next slot / next stock 指標。
- 原因: 可精確做到每拍換 1 格，且避免整頁重排。
- 替代方案: 保留 page flip 並加動畫；不採用，因為仍是整批跳動。

2. UI 刷新與 quote 抓取解耦。
- 決策: 固定 UI timer 每 2 秒運作，即使無新 quote 也執行輪巡替換。
- 原因: 符合需求「固定 UI 節拍刷新」。
- 替代方案: 僅在新 quote 到達刷新；不採用，因為節奏不穩定。

3. 色彩以「當前顯示內容」為準。
- 決策: 每次 slot 套用資料時，依該資料 `change_percent` 同步更新文字色與邊框色。
- 原因: 確保色彩語意與畫面顯示一致，不依背景快取狀態漂移。
- 替代方案: 只在 quote 到達時改色；不採用，因輪巡替換時會出現色值與內容短暫不一致。

4. 返回 Dashboard 重置輪巡。
- 決策: 進入 Dashboard 時重建初始 slot 映射，從清單第 1 檔開始。
- 原因: 行為可預期，符合既定偏好。
- 替代方案: 保留離開前指標；不採用。

## Risks / Trade-offs

- [Risk] 輪巡指標與股票清單變更不同步導致越界。
  → Mitigation: 在 card count / stock list reload 時重算 slot mapping 與指標。
- [Risk] 非 active screen 仍刷新導致不必要重繪。
  → Mitigation: timer callback 僅在 Dashboard active 時做實際 render。
- [Risk] 固定節拍刷新增加少量 UI 負載。
  → Mitigation: 每拍只更新 1 格，避免整屏重繪。

## Migration Plan

1. 先保留舊常數，新增新常數與新資料結構。
2. 切換 render 路徑為 slot-based，移除 page flip 邏輯。
3. 接上 screen enter/leave hook 處理重置。
4. 實機驗證 <=5 與 >5 的場景，再移除不再使用的舊狀態變數。

## Open Questions

- 未來是否要在 Settings 提供 UI 輪巡節拍設定（目前固定 2 秒）。
