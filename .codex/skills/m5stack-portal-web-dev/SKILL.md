---
name: m5stack-portal-web-dev
description: M5Stack Portal 前端網頁協作技能，專責 components/portal_backend/portal 內的 HTML/CSS/JS/靜態資源調整。當需求涉及 portal UI 版面、互動、文案與可用性優化時使用；若需要 firmware 或 API 行為變更，先完成前端可獨立部分後交接 m5stack-core2-dev。
---

# M5Stack Portal Web Dev

## Overview

專責 `components/portal_backend/portal` 的前端頁面與靜態資源調整。
目標是用最小改動完成 Portal UI 需求，並保持與既有 firmware 分工清楚。

## Trigger

當需求包含以下任一類型時使用本技能：
- Portal 網頁版面、樣式、互動（HTML/CSS/JS）
- Portal 文案、表單、可用性與流程優化
- `components/portal_backend/portal` 內靜態資源調整

## Boundaries

- 允許：`components/portal_backend/portal` 內前端檔案與必要資源。
- 不處理：`flash.sh`、燒錄、monitor、序列埠與連線問題（交給 `m5stack-core2-flash-agent`）。
- 不得修改 firmware/API 行為；若需求依賴 firmware/API 變更，需輸出 handoff note 後轉交 `m5stack-core2-dev`。

## Workflow

1. 先定位需求對應頁面與資源，確認是否能在 portal 前端內獨立完成。
2. 先明確寫出使用者主訴屬於尺寸、間距、風格、可用性中的哪一類；若同時存在多類問題，需拆開處理，不得混成單一重設計。
3. 提出至少 2 個前端可行方案，列出取捨並推薦 1 個方案，再進行實作。
4. 若使用者明確指出元件「太大」，方案必須優先縮小尺寸與降低首屏佔用，不得用增加留白、放大元件或加重容器存在感作為主要解法。
5. 若重設計的是導覽列或選單，需先檢查其首屏高度占比與主內容層級，避免導覽比內容更搶眼。
6. 採最小可行改動，避免順手重構無關區塊。
7. 完成後依 DoD 驗收，並提供可重現的手動驗證步驟（桌面/手機瀏覽、主要流程）。
8. 若需跨到 firmware/API 才能完成，先交付前端可完成部分，再產出交接清單。

## Definition of Done (DoD)

- RWD：在 `360px`、`768px`、`1280px` 寬度下，主要流程可用且無水平捲動。
- 可用性：手機可用觸控完成主要流程。
- 狀態覆蓋：成功、載入中、錯誤至少有基本可理解的 UI 呈現。
- 基本效能檢查：避免明顯阻塞主流程的前端行為（如不必要的大量同步運算）。
- 程式品質：避免冗餘寫法與無實際作用程式碼（dead code、重複邏輯、無效監聽與未使用資源）。
- 版型檢查：`width: 100%` 的 `a`、`button`、`input` 等元件需確認 `box-sizing` 正確，避免 padding/border 造成溢出、互壓或假性間距失效。

## Response Template

每次回覆需固定包含以下 4 段：
- `變更檔案`
- `互動行為`
- `驗收步驟`
- `風險與未完成項`

## Handoff

符合以下任一條件時，轉交 `m5stack-core2-dev`：
- 需要新增或變更 API 行為、資料格式、儲存欄位
- 需要改動 `portal_backend/portal` 以外的韌體邏輯
- 需求本質已超出前端頁面層
