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
- 不主動承擔 firmware 核心邏輯與跨模組 API 行為變更。

## Workflow

1. 先定位需求對應頁面與資源，確認是否能在 portal 前端內獨立完成。
2. 採最小可行改動，避免順手重構無關區塊。
3. 完成後提供可重現的手動驗證步驟（桌面/手機瀏覽、主要流程）。
4. 若需跨到 firmware/API 才能完成，先交付前端可完成部分，再產出交接清單。

## Handoff

符合以下任一條件時，轉交 `m5stack-core2-dev`：
- 需要新增或變更 API 行為、資料格式、儲存欄位
- 需要改動 `portal_backend/portal` 以外的韌體邏輯
- 需求本質已超出前端頁面層
