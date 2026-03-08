---
name: m5stack-portal-web-dev
description: 僅用於 components/portal_backend/portal 路徑內的前端調整（HTML/CSS/JS/靜態資源）。當使用者提到該路徑下的檔案、portal 頁面樣式、portal UI 互動或文案時使用。其他前端需求、firmware 邏輯、燒錄流程均不屬於本 skill 範圍。
---

Routing: m5stack-portal-web-dev

## Overview

專責 `components/portal_backend/portal` 的前端頁面與靜態資源調整。
目標是用最小改動完成 Portal UI 需求，並保持與既有 firmware 分工清楚。

## Boundaries

- 允許：`components/portal_backend/portal` 內前端檔案與必要資源。
- 不處理：`flash.sh`、燒錄、monitor、序列埠與連線問題（交由 `m5stack-core2-flash-agent`）。
- 不主動承擔 firmware 核心邏輯與跨模組 API 行為變更（交給 `m5stack-core2-dev`）。

## Workflow

1. 先定位需求對應頁面與資源，確認是否能在 portal 前端內獨立完成。
2. 採最小可行改動，避免順手重構無關區塊。
3. 完成後提供可重現的手動驗證步驟（桌面/手機瀏覽、主要流程）。
4. 若需跨到 firmware/API 才能完成，先交付前端可完成部分，再產出交接清單。

## 驗證協作策略

前端改完後若需要 build/flash/monitor 驗證，交由 `m5stack-core2-flash-agent` 執行，本技能只保留驗證摘要。

| 操作 | 做法 |
|------|------|
| 前端修改後驗證 build | 交由 `m5stack-core2-flash-agent` 執行 `./flash.sh --build-only`，只接收成功/失敗摘要 |
| 需要實機驗證 UI | 交由 `m5stack-core2-flash-agent` 依序執行 `--app-flash` 與 `--monitor` 擷取啟動 log |
| 多頁面同時調整 | 主流程直接改（靜態檔案小，不需拆分） |

## Handoff

符合以下任一條件時，轉交 `m5stack-core2-dev`：
- 需要新增或變更 API 行為、資料格式、儲存欄位
- 需要改動 `portal_backend/portal` 以外的韌體邏輯
- 需求本質已超出前端頁面層
