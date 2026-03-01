## ADDED Requirements

### Requirement: GET /ai 回傳 AI 設定 JSON
`GET /ai` SHALL 回傳目前 NVS 中儲存的 AI 設定，格式為 JSON，包含 `gemini_key`、`claude_key`、`openai_key`、`prompt_template` 四個欄位，值為 string（空字串表示未設定）。

#### Scenario: 正常查詢
- **WHEN** 瀏覽器呼叫 `GET /ai`
- **THEN** 回傳 `{"gemini_key":"...","claude_key":"...","openai_key":"...","prompt_template":"..."}`，HTTP 200

#### Scenario: 尚未設定任何 key
- **WHEN** NVS 中無任何 AI 設定
- **THEN** 回傳所有欄位為空字串的 JSON，HTTP 200

### Requirement: POST /ai 儲存 AI 設定並回傳 JSON
`POST /ai` SHALL 讀取 `application/x-www-form-urlencoded` body，將 `gemini_key`、`claude_key`、`openai_key`、`prompt_template` 分別寫入 NVS，並依第一個非空 key 的順序（Gemini > Claude > OpenAI）更新 active provider。成功後 SHALL 回傳 JSON `{"ok":true}`，失敗（body 解析錯誤）SHALL 回傳 HTTP 400 `{"ok":false}`。回應 Content-Type SHALL 為 `application/json`。

#### Scenario: 儲存成功
- **WHEN** 瀏覽器送出含有效欄位的 POST body
- **THEN** 各 key 寫入 NVS，回傳 `{"ok":true}`，HTTP 200，Content-Type: application/json

#### Scenario: body 解析失敗
- **WHEN** request body 為空或超過 4096 bytes
- **THEN** 回傳 HTTP 400，`{"ok":false}`

### Requirement: Portal 前端 AI Settings tab AJAX 提交
Portal 網頁 AI Settings tab 的 Save 按鈕 SHALL 以 `fetch()` AJAX 方式提交表單，不得觸發 full-page navigation。提交期間 Save 按鈕 SHALL 為 disabled。收到 server 回應後，SHALL 在 `ai_msg` 元素顯示結果訊息；成功顯示綠色文字，失敗顯示紅色文字。

#### Scenario: 儲存成功
- **WHEN** 使用者填入 API key 並點擊 Save
- **THEN** 頁面不跳轉，`ai_msg` 顯示綠色成功訊息，Save 按鈕恢復可點擊

#### Scenario: 儲存失敗（網路或 server 錯誤）
- **WHEN** fetch 發生錯誤或 server 回傳 `{"ok":false}`
- **THEN** 頁面不跳轉，`ai_msg` 顯示紅色錯誤訊息，Save 按鈕恢復可點擊

#### Scenario: 提交中防重複
- **WHEN** 使用者點擊 Save 後 request 尚未完成
- **THEN** Save 按鈕為 disabled，無法再次觸發提交
