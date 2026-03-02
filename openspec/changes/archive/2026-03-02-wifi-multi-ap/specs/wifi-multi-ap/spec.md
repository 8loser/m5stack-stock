## ADDED Requirements

### Requirement: 多 AP NVS 儲存格式

系統 SHALL 在 NVS `wifi_cfg` namespace 以下列 key 儲存多 AP 資料：`ap_count`（u8，已儲存數量）、`ap_N_ssid`（str，SSID，N=0..4）、`ap_N_pass`（str，密碼，N=0..4）。最多儲存 5 組（`WIFI_MAX_AP_COUNT=5`）。所有 key 長度 SHALL ≤ 15 字元（NVS 限制）。

#### Scenario: 讀取 AP 數量
- **WHEN** 呼叫 `storage_wifi_ap_count()`
- **THEN** 回傳 NVS 中 `ap_count` 的值；若 key 不存在回傳 0

#### Scenario: 儲存指定 slot
- **WHEN** 呼叫 `storage_wifi_save_ap(idx, ssid, pass)`，idx < WIFI_MAX_AP_COUNT
- **THEN** 寫入 `ap_N_ssid` 與 `ap_N_pass`，commit 成功回傳 ESP_OK；不修改 `ap_count`

#### Scenario: 讀取指定 slot
- **WHEN** 呼叫 `storage_wifi_load_ap(idx, ssid, ssid_sz, pass, pass_sz)`
- **THEN** 將對應 slot 的 SSID 與密碼寫入呼叫者提供的 buffer；buffer 需各自至少 WIFI_SSID_MAX_LEN（33）/ WIFI_PASS_MAX_LEN（65）bytes

### Requirement: storage_wifi_add_ap upsert 語義

`storage_wifi_add_ap(ssid, pass)` SHALL 實作 upsert：若已有相同 SSID 則更新密碼並回傳；若清單已滿（count >= WIFI_MAX_AP_COUNT）則先移除 index 0（最舊），再寫入新 AP 至末尾並將 `ap_count` 加 1，一次 commit。

#### Scenario: 新增全新 AP
- **WHEN** 呼叫 `storage_wifi_add_ap("HomeWifi", "pw1234")`，清單中無此 SSID 且 count < 5
- **THEN** 寫入至 slot count，`ap_count` 加 1，回傳 ESP_OK

#### Scenario: 更新已存在 SSID 的密碼
- **WHEN** 呼叫 `storage_wifi_add_ap("HomeWifi", "newpw")`，"HomeWifi" 已在 slot 2
- **THEN** 只更新 slot 2 的 pass，`ap_count` 不變，回傳 ESP_OK

#### Scenario: 清單已滿時淘汰最舊
- **WHEN** 清單已有 5 組 AP，呼叫 `storage_wifi_add_ap("NewAP", "pw")`
- **THEN** 移除 index 0（最舊），其餘往前移動，新 AP 寫入 slot 4，`ap_count` 維持 5

### Requirement: storage_wifi_remove_ap slot shifting

`storage_wifi_remove_ap(idx)` SHALL 在同一個 READWRITE handle 中：將 idx+1 至 count-1 的槽位依序往前覆蓋，清除最後一格的兩個 key，將 `ap_count` 減 1，一次 commit。

#### Scenario: 移除中間項目
- **WHEN** count=3，呼叫 `storage_wifi_remove_ap(1)`
- **THEN** slot 0 不變，slot 2 的資料移至 slot 1，slot 2 的 key 被清除，`ap_count` 變為 2

#### Scenario: 移除唯一項目
- **WHEN** count=1，呼叫 `storage_wifi_remove_ap(0)`
- **THEN** slot 0 的 key 被清除，`ap_count` 變為 0

### Requirement: storage_wifi_migrate_legacy 冪等遷移

`storage_wifi_migrate_legacy()` SHALL 於 `storage_init()` 後、`wifi_manager_init()` 前由 main.c 呼叫。行為：若 `ap_count` key 已存在 → 直接回傳（已遷移）；若舊 `ssid` key 存在 → 讀取 `ssid`/`password`，寫入 slot 0，設 `ap_count=1`，刪除舊 key，一次 commit；若兩者均不存在 → 設 `ap_count=0`，回傳。

#### Scenario: 全新裝置（無任何 WiFi 資料）
- **WHEN** NVS 中無 `ap_count` 也無舊 `ssid`
- **THEN** 寫入 `ap_count=0`，回傳 ESP_OK，不修改其他 key

#### Scenario: 舊版單筆格式遷移
- **WHEN** NVS 中有舊 `ssid="HomeWifi"` 與 `password="pw"`，無 `ap_count`
- **THEN** 寫入 `ap_0_ssid="HomeWifi"`、`ap_0_pass="pw"`、`ap_count=1`，刪除舊 `ssid` 與 `password` key，log 遷移訊息

#### Scenario: 已是新格式（冪等）
- **WHEN** NVS 中已有 `ap_count` key
- **THEN** 直接回傳 ESP_OK，不做任何修改

### Requirement: wifi_manager_connect_any_saved 自動輪試

`wifi_manager_connect_any_saved()` SHALL 依序讀取 index 0 至 count-1 的已儲存 AP，對每一組呼叫 `wifi_manager_connect(ssid, pass)`（blocking，30 秒逾時）。第一個成功（回傳 ESP_OK）即停止並回傳 ESP_OK。全部失敗後呼叫 `notify_state(WIFI_STATE_FAILED)` 並回傳 ESP_FAIL。count=0 時呼叫 `notify_state(WIFI_STATE_DISCONNECTED)` 並回傳 ESP_ERR_NOT_FOUND。

`wifi_manager_connect_saved()` SHALL 改為直接呼叫並回傳 `wifi_manager_connect_any_saved()`。

#### Scenario: 第一個 AP 連線成功
- **WHEN** 有 2 組已儲存 AP，第一組連線成功
- **THEN** 回傳 ESP_OK，不嘗試第二組

#### Scenario: 第一個失敗、第二個成功
- **WHEN** 有 2 組已儲存 AP，第一組失敗，第二組成功
- **THEN** 嘗試第二組後回傳 ESP_OK

#### Scenario: 全部失敗
- **WHEN** 所有已儲存 AP 均連線失敗
- **THEN** 呼叫 `notify_state(WIFI_STATE_FAILED)`，回傳 ESP_FAIL

#### Scenario: 無已儲存 AP
- **WHEN** `storage_wifi_ap_count()` = 0
- **THEN** 呼叫 `notify_state(WIFI_STATE_DISCONNECTED)`，回傳 ESP_ERR_NOT_FOUND

### Requirement: GET /saved_aps 查詢已儲存 AP 清單

`GET /saved_aps` SHALL 回傳 JSON 陣列，每筆僅含 `ssid` 欄位，不回傳密碼。buffer 使用 256 bytes stack 空間（5 AP × ~44 bytes + overhead）。

#### Scenario: 有已儲存 AP
- **WHEN** 瀏覽器呼叫 `GET /saved_aps`，NVS 中有 2 組 AP
- **THEN** 回傳 `[{"ssid":"HomeWifi"},{"ssid":"Office"}]`，HTTP 200

#### Scenario: 無已儲存 AP
- **WHEN** NVS 中 `ap_count=0`
- **THEN** 回傳 `[]`，HTTP 200

### Requirement: POST /saved_aps/remove 移除指定 AP

`POST /saved_aps/remove` 接受 `application/x-www-form-urlencoded` body，欄位 `ssid`。SHALL 遍歷已儲存 AP，找到匹配 SSID 後呼叫 `storage_wifi_remove_ap(idx)`。找到回傳 `{"ok":true}`（HTTP 200）；找不到回傳 HTTP 404 `{"ok":false,"error":"not_found"}`；body 格式錯誤回傳 HTTP 400。

#### Scenario: 移除成功
- **WHEN** POST body 為 `ssid=HomeWifi`，且 "HomeWifi" 在已儲存清單中
- **THEN** 從 NVS 移除，回傳 `{"ok":true}`，HTTP 200

#### Scenario: SSID 不存在
- **WHEN** POST body 為 `ssid=UnknownAP`，清單中無此 SSID
- **THEN** 回傳 HTTP 404 `{"ok":false,"error":"not_found"}`

### Requirement: Portal WiFi tab Saved Networks 區塊

Portal WiFi tab SHALL 在 Connect 表單下方顯示「Saved Networks」區塊（`<div id='saved_aps_list'>`），列出所有已儲存 SSID，每筆附 Remove 按鈕。切換至 WiFi tab 時 SHALL 呼叫 `loadSavedAps()` 刷新清單。掃描結果中已儲存的 SSID SHALL 顯示 `[Saved]` badge。

#### Scenario: 顯示已儲存清單
- **WHEN** 使用者切換至 WiFi tab
- **THEN** 頁面呼叫 `GET /saved_aps` 並渲染清單，每筆含 SSID 與 Remove 按鈕

#### Scenario: Remove 按鈕移除 AP
- **WHEN** 使用者點擊某 SSID 的 Remove 按鈕
- **THEN** 呼叫 `POST /saved_aps/remove`，成功後清單刷新，該筆消失

#### Scenario: 掃描結果顯示 Saved badge
- **WHEN** 掃描到已儲存的 SSID（如 "HomeWifi"）
- **THEN** 下拉選單中顯示 "HomeWifi [Saved]  (-65dBm)"

### Requirement: Connect 表單空密碼自動帶入

`portal_wifi_post_handler` 在 `free(body)` 後、建立連線 task 前，若 `conn_req->password` 為空字串且 SSID 不為空，SHALL 遍歷已儲存 AP 找到匹配 SSID 並將對應密碼填入 `conn_req->password`。

#### Scenario: 密碼留空連線已儲存 AP
- **WHEN** 使用者在 Portal 選擇已儲存的 "HomeWifi" 但密碼欄留空後按 Connect
- **THEN** handler 自動帶入已儲存密碼，連線正常進行

#### Scenario: 密碼有輸入時不覆蓋
- **WHEN** 使用者輸入 SSID 與新密碼後按 Connect
- **THEN** 使用使用者輸入的密碼，不從 NVS 讀取
