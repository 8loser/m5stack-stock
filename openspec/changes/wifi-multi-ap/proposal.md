## Why

裝置目前的 NVS 只能儲存一組 WiFi 認證（`wifi_cfg/ssid` + `wifi_cfg/password`），在多個場所使用時（家、公司、行動熱點）必須每次手動重新配網。本次變更讓裝置支援最多 5 組 AP，開機時自動輪試，並透過 Portal 頁面管理已儲存的 AP 清單。

## What Changes

- **NVS 新格式**：`wifi_cfg/ap_count`（u8）+ `wifi_cfg/ap_N_ssid` / `wifi_cfg/ap_N_pass`（N = 0–4），最多 5 組
- **冪等遷移**：開機時自動將舊格式單筆 `ssid/password` 遷移至 slot 0，不需清除 NVS
- **自動輪試**：`wifi_manager_connect_saved()` 改為依序嘗試所有已儲存 AP，第一個成功即停止
- **Portal Saved Networks**：WiFi tab 新增「Saved Networks」區塊，顯示已儲存 SSID 清單及 Remove 按鈕
- **Portal API**：新增 `GET /saved_aps`、`POST /saved_aps/remove` 兩個 HTTP endpoint
- **Portal 掃描標示**：掃描結果中已儲存的 SSID 顯示 `[Saved]` badge
- **空密碼自動填入**：Connect 表單密碼留空時，自動帶入已儲存的對應密碼

## Capabilities

### New Capabilities

- `wifi-multi-ap`: NVS 多 AP 儲存（storage layer）、開機自動輪試連線、Portal 管理介面（Saved Networks 清單、Remove、掃描 badge）

### Modified Capabilities

（無現有 spec 的行為需要變更）

## Impact

| 範圍 | 說明 |
|------|------|
| `components/storage/` | 新增 6 個 API：`ap_count`、`save_ap`、`load_ap`、`remove_ap`、`add_ap`、`migrate_legacy` |
| `components/wifi_manager/` | 修改 `connect_saved()`、新增 `connect_any_saved()`、新增兩個 HTTP handler、更新 Portal HTML |
| `main/main.c` | 在 `storage_init()` 後插入一行 `storage_wifi_migrate_legacy()` |
| `include/app_config.h` | 新增 `WIFI_MAX_AP_COUNT`、`WIFI_SSID_MAX_LEN`、`WIFI_PASS_MAX_LEN` 三個常數 |
| NVS `wifi_cfg` namespace | 新增 key：`ap_count`、`ap_0_ssid`～`ap_4_ssid`、`ap_0_pass`～`ap_4_pass`；舊 key `ssid`/`password` 在遷移後清除 |
| **BREAKING** | NVS `wifi_cfg` 格式改變，需 `storage_wifi_migrate_legacy()` 處理升級；未遷移裝置無法直接讀取新格式 |
