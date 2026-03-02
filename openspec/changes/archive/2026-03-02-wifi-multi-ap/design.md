## Context

目前 `wifi_cfg` NVS namespace 只有兩個 key：`ssid`（str）與 `password`（str），對應單一 AP。`wifi_manager_connect_saved()` 直接讀取這兩個 key 進行連線。Portal 的 WiFi tab 提供 Connect 表單，成功後呼叫 `storage_wifi_save()` 覆蓋舊值。

本次需要在不清除 NVS 的前提下升級至多 AP 格式，同時新增 Portal 管理介面。

## Goals / Non-Goals

**Goals:**
- NVS 最多儲存 5 組 AP，超過時自動淘汰最舊的（index 0）
- 開機自動依序嘗試所有已儲存 AP
- Portal 可查看已儲存清單、刪除單筆
- 舊版單筆格式自動遷移，裝置無需 erase flash

**Non-Goals:**
- AP 優先順序 UI（固定為 FIFO，先存先試）
- WPA Enterprise / 企業認證
- 背景連線重試迴圈（每次呼叫為單次 pass）
- Portal 編輯密碼（只能新增或刪除）

## Decisions

### D1：NVS Key 命名 `ap_N_ssid` / `ap_N_pass`

NVS key 上限 15 字元。選擇 `ap_0_ssid`（9 字元）…`ap_4_pass`（9 字元），5 組均符合限制。另新增 `ap_count`（8 字元）記錄已存數量。

放棄 blob 方式（整批序列化）：NVS str 可直接讀寫個別欄位，debug 與修復更容易。

### D2：遷移以 `ap_count` key 是否存在作為判斷依據

冪等策略：開機時若 `ap_count` 已存在 → 已是新格式，直接跳過。若不存在且舊 `ssid` 存在 → 寫入 slot 0、設 `ap_count=1`、刪除舊 key，一次 commit。若兩者都不存在 → 初始化 `ap_count=0`。

放棄版本號方式：僅一次遷移，不需版本欄位增加複雜度。

### D3：刪除採用 Slot Shifting（往前移動）

移除 index i 時，將 i+1…count-1 逐個複製至前一格，再清除最後一格，並將 `ap_count` 減 1，一次 commit。

選擇此方式而非「標記 tombstone」：NVS erase key 成本低，且清單最多 5 筆，O(n) 可接受。好處是 index 永遠連續，讀取迴圈不需跳過空洞。

### D4：`storage_wifi_add_ap()` 合併新增與更新

呼叫時先掃描現有 SSID，若重複則只更新密碼（等同 upsert）。若清單已滿（5 筆），先移除 index 0（最舊），再寫入。`wifi_manager_connect()` 成功後從 `storage_wifi_save()` 改呼叫 `storage_wifi_add_ap()`。

### D5：`GET /saved_aps` 不回傳密碼

只回傳 SSID 陣列 `[{"ssid":"..."}]`，避免密碼透過 HTTP 明文暴露（Portal 為本地 AP，但仍遵循最小揭露原則）。

### D6：Connect 表單空密碼時自動帶入已儲存密碼

使用者點掃描列表中已儲存的 AP 時，密碼欄可留空。`portal_wifi_post_handler` 在 `free(body)` 後、執行連線前，若 `conn_req->password` 為空則查 NVS 填入對應密碼。

## Risks / Trade-offs

| 風險 | 緩解措施 |
|------|---------|
| 斷電於遷移 commit 前 | 下次開機 `ap_count` 仍不存在，重新執行遷移；舊 `ssid` key 若也消失則初始化 `ap_count=0`（使用者需重新配網，可接受） |
| NVS 空間 | 5 AP × (33+65 bytes) ≈ 490 bytes 資料 + NVS entry overhead ≈ 2KB；預設 NVS partition 24KB，充裕 |
| Portal HTTP stack size | 兩個新 handler 均使用 stack 上的小 buffer（≤256 bytes），無 heap 額外分配壓力 |
| Slot shifting 中途 NVS 寫入失敗 | 中途失敗會留下部分移動後的殘留資料；修改 `ap_count` 放在最後 commit，確保讀取端看到的數量一致 |

## Migration Plan

1. 燒錄新韌體（舊 NVS 保留）
2. 開機執行 `storage_wifi_migrate_legacy()`：舊單筆資料自動搬移至 slot 0
3. 無需使用者介入；若需清除所有 AP，可至 Portal 逐一 Remove 或 `./flash.sh --erase`

回滾：降版韌體無法讀取新格式（`ap_count` key 存在但無舊 `ssid` key），連線會失敗並進入 Portal 要求重新配網。接受此行為（降版屬非正常使用情境）。
