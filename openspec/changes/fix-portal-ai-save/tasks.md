## 1. 後端：修改 portal_ai_post_handler 回傳 JSON

- [ ] 1.1 將 `portal_ai_post_handler` 的成功回應從 `httpd_resp_sendstr` HTML 改為 `send_json_response(req, 200, "{\"ok\":true}")`
- [ ] 1.2 將 body 解析失敗的錯誤回應改為回傳 JSON `{"ok":false}`（已有 `httpd_resp_send_err` 改用 `send_json_response`）

## 2. 前端：AI Settings 表單改 AJAX 提交

- [ ] 2.1 為 AI Settings `<form>` 加上 `id='ai_form'`（或直接用 JS 抓 `action='/ai'` 的 form）
- [ ] 2.2 移除 `<form method='post' action='/ai'>` 的 method/action 屬性（或在 JS 中 `preventDefault`），改由 JS 控制提交
- [ ] 2.3 新增 JS 函式 `saveAiSettings()`：以 `FormData` 收集欄位、`URLSearchParams` 序列化，透過 `fetch('/ai', {method:'POST', body:...})` 送出
- [ ] 2.4 Save 按鈕在 fetch 送出前設 `disabled=true`，收到回應後恢復 `disabled=false`
- [ ] 2.5 成功（`res.ok && data.ok`）時於 `ai_msg` 顯示綠色成功訊息；失敗時顯示紅色錯誤訊息

## 3. 驗證

- [ ] 3.1 build 確認無編譯錯誤（`./flash.sh --build-only`）
- [ ] 3.2 實機測試：在 AI Settings tab 修改 API key，點擊 Save，確認頁面不跳轉且 `ai_msg` 顯示成功
- [ ] 3.3 實機測試：重新整理頁面，確認表單欄位顯示剛才儲存的值（驗證 NVS 寫入生效）
