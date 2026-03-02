## Why

AI 應用機制尚未確定，先移除所有 AI 執行邏輯（timer、HTTP 呼叫、result queue），避免無效資源佔用。Portal web 保留 AI tab，讓使用者仍可預先填入 API key，待機制確認後直接啟用。

## What Changes

- **BREAKING** 移除 `ai_provider` 元件從編譯（目錄保留，不刪除）
- 移除 scheduler 中的 AI timer 與 `do_ai_analysis()` 邏輯
- 移除 `g_ai_result_queue` 及 main loop 中的 AI 結果消費
- 移除 `ai_provider_init()` 呼叫
- 保留 portal web 的 AI 設定 tab（`/ai` GET/POST 路由），API key 仍可儲存至 NVS
- 移除 screen_info 中的 AI 資訊顯示區塊（無 ai_provider 可查詢）
- 移除 `ui_manager_log_ai()` 函式
- 從 `schedule_config_t` 移除 `ai_interval_min` 欄位
- 保留 storage 中的 `storage_ai_*` 函式（portal 儲存 API key 仍需使用）
- 移除 `app_config.h` 中的 AI 相關常數

## Capabilities

### New Capabilities

（無新增 capability）

### Modified Capabilities

- `scheduler`: 移除 `ai_result_queue` 參數與 AI timer，`scheduler_init` 簽名變更
- `portal-stock-management`: Portal HTML 保留 AI tab，`/ai` GET/POST 路由繼續提供 API key 讀寫
- `info-page`: screen_info 移除 AI 資訊區塊，sections 重新編號
- `settings-page`: `schedule_config_t` 移除 `ai_interval_min`，NVS key `ai_ivl` 不再讀寫
- `main`: 移除 AI queue、初始化、main loop AI 消費

## Impact

| 類別 | 影響 |
|------|------|
| 元件移除 | `ai_provider` 從所有 CMakeLists.txt REQUIRES 移除 |
| API 破壞 | `scheduler_init(queue, ai_queue)` → `scheduler_init(queue)` |
| NVS | `ai_cfg` namespace 繼續讀寫（portal AI tab 儲存機制保留）|
| Portal | `/ai` route 保留，AI tab 可存取 |
| Binary 大小 | 預期減少（移除 3 個 HTTP provider 實作） |
