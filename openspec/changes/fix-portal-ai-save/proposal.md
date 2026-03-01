## Why

Portal 網頁的 AI Provider 設定頁面使用傳統 HTML form POST，提交後瀏覽器跳轉至獨立頁面，使用者若按瀏覽器「上一頁」會從 bfcache 還原舊畫面，導致表單欄位顯示舊值，誤以為儲存未生效。`ai_msg` div 存在但從未被更新，也無任何 inline 回饋。

## What Changes

- `POST /ai` handler 改回傳 JSON `{"ok":true}` 取代 HTML 跳轉頁
- 前端 AI 表單改為 `fetch()` AJAX 提交，提交後在頁面原地顯示成功/失敗訊息，不做頁面跳轉
- `ai_msg` div 正確顯示儲存結果（成功綠字、失敗紅字）
- Save 按鈕在請求進行中改為 disabled，防止重複提交

## Capabilities

### New Capabilities

- `portal-ai-settings`: Portal 網頁 `/ai` GET/POST API 規格，以及前端 AI Settings tab 的 AJAX 互動行為

### Modified Capabilities

（無既有 spec 需修改）

## Impact

- `components/wifi_manager/wifi_manager.c`：`portal_ai_post_handler` 回傳格式改 JSON；前端 HTML/JS inline 字串修改 AI form submit 邏輯
- 無 API breaking change（`GET /ai` response 格式不變，`POST /ai` request 格式不變，僅 response 從 HTML 改 JSON）
- 不影響其他元件
