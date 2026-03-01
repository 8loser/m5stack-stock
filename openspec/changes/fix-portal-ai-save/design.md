## Context

Portal 網頁的 AI Settings tab 目前使用 `<form method='post' action='/ai'>`，提交後由瀏覽器執行 full-page POST，server 回傳 HTML 頁面導致頁面跳轉。使用者若按瀏覽器「上一頁」會從 bfcache 還原舊畫面而非重新 fetch，導致誤判「儲存未生效」。`ai_msg` div 存在但從未被 JS 更新。

現有後端（`portal_ai_post_handler`）讀取 `application/x-www-form-urlencoded` body，邏輯正確。修改範圍集中在 `wifi_manager.c` 內嵌的 HTML/JS 字串。

## Goals / Non-Goals

**Goals:**
- AI Settings 表單改用 AJAX 提交，不做頁面跳轉
- 儲存結果（成功 / 失敗）inline 顯示於 `ai_msg` div
- Save 按鈕在 request 進行中 disabled，防止重複提交
- `POST /ai` response 從 HTML 改為 JSON `{"ok":true}`

**Non-Goals:**
- 不修改 Provider 自動選擇邏輯（first non-empty key wins）
- 不修改 `GET /ai` response 格式
- 不修改 POST body 格式（維持 `application/x-www-form-urlencoded`）
- 不新增 provider 手動選擇 UI

## Decisions

### Decision 1：POST body 格式維持 form-encoded，不改 JSON

**選擇**：前端 AJAX 仍送 `application/x-www-form-urlencoded`，後端 `get_form_value` 解析邏輯不動。

**理由**：改 JSON 需改後端解析（引入 cJSON 或另寫 parser），增加風險。form-encoded 格式不變，只改 response 格式，改動最小。

**替代方案考慮**：改 JSON body → 後端要改 read + parse，較複雜，無明顯優勢。

### Decision 2：前端用 `FormData` + `URLSearchParams` 序列化

**選擇**：JS 中以 `new FormData(form)` 收集欄位，`new URLSearchParams(fd).toString()` 序列化，`fetch()` 送出。

**理由**：自動收集所有 `name` 欄位，不需逐欄位手動取值，維護成本低。

### Decision 3：`POST /ai` 成功回 JSON `{"ok":true}`，失敗回 `{"ok":false}`

**選擇**：後端 handler 改為 `send_json_response(req, 200, "{\"ok\":true}")`。

**理由**：與其他 endpoint（`/stocks/add`、`/stocks/remove`）格式一致，前端可統一處理。

## Risks / Trade-offs

- **舊版瀏覽器不支援 `fetch`**：Portal 預設在手機/電腦現代瀏覽器使用，風險極低，可忽略。
- **HTML 字串修改難維護**：`wifi_manager.c` 內嵌 HTML/JS 字串本身就難讀，本次修改集中在 AI form 區段，範圍有限。
- **無 migration**：僅改 response 格式，client 端同步更新，無相容性問題。

## Open Questions

（無）
