## Context

Portal 前端在載入時同時觸發多個非同步請求，現況由各區塊各自處理成功/失敗，缺少全域初始化完成條件與超時恢復策略。結果是使用者在資料未回來前看到空白內容，或在慢網路下誤判畫面異常。

## Goals / Non-Goals

**Goals:**
- 提供全頁一致的首次載入狀態（loading）
- 提供逾時可理解狀態與可操作重試（timeout error + retry）
- 確保重試會重跑全部初始化請求並避免狀態競態

**Non-Goals:**
- 不新增或修改任何後端 HTTP API
- 不重構既有每個資料區塊的渲染結構
- 不引入外部前端依賴

## Decisions

1. 採全頁 overlay，而非分區塊 loading
- Why: 首次載入時資訊密度高，單一全頁狀態可減少使用者誤解。
- Alternative: 分區塊 loading。缺點是體驗碎片化，仍可能誤認空白區塊為無資料。

2. 逾時後顯示錯誤面板並提供 Retry，不直接關閉遮罩
- Why: 直接關閉會讓使用者誤判為「就是沒資料」。
- Alternative: 逾時自動隱藏。缺點是狀態語意不清。

3. Retry 重抓全部初始化請求
- Why: 避免跨區塊資料版本不一致，行為可預期。
- Alternative: 只重抓失敗項目。缺點是需要額外追蹤部分成功狀態，複雜度提高。

4. 新增初始化輪次（generation）防止舊請求覆蓋新重試結果
- Why: 使用者連續重試時，先前慢回應不可覆蓋當前 overlay state。
- Alternative: 不做輪次保護。缺點是存在 race condition。

## Risks / Trade-offs

- [初始化任務計數錯誤導致遮罩不消失] → 透過統一 task wrapper 在成功/失敗路徑都遞減計數
- [連續重試產生競態] → 使用 generation token，僅接受當前輪次更新
- [overlay 阻擋互動影響可用性] → 只在初始化與 timeout_error 顯示，完成即切 hidden
- [慢網路下誤觸發逾時] → timeout 設為可調常數（預設 10000ms），可後續微調

## Migration Plan

1. 在 Portal `index.html` 新增 overlay HTML/CSS 與狀態 class
2. 將初始化請求收斂進 bootstrap 流程與 timeout 管理
3. 加入 retry handler，重跑全部初始化集合
4. 驗證正常/失敗/逾時/連續重試情境
5. 若需 rollback，移除 overlay 與 bootstrap 狀態機，恢復既有分散載入流程

## Open Questions

- 無（本次 UX 路徑已定案：逾時顯示全頁錯誤面板 + 重試，且重試重抓全部）
