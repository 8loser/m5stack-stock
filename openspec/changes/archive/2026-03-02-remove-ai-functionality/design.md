## Context

目前專案整合了完整的 AI 分析流程：`ai_provider` 元件（含 Gemini/Claude/OpenAI 三個 HTTP provider）、scheduler 的 AI timer、`g_ai_result_queue`、portal web AI tab，以及 screen_info 的 AI 顯示。AI 機制尚未確定，這些代碼在每次開機都會初始化但從未產生有效輸出，造成資源浪費。

此 change 是純移除（removal），不引入新行為，目標是讓程式碼回到只做「股票報價顯示 + portal 設定」的最小可用狀態。

## Goals / Non-Goals

**Goals:**
- 移除所有 AI 執行路徑，確保 AI timer 不再觸發、HTTP 不再發出
- Portal web 保持可用，WiFi、AI、Stocks 三個 tab 均保留；AI tab 僅提供 API key 儲存，不觸發執行
- `ai_provider/` 目錄保留在磁碟（不 `git rm`），只從 build 系統移除
- 編譯後 binary 無任何 AI 相關符號

**Non-Goals:**
- 不修改 AI provider 的邏輯（保留供未來使用）
- 不移除 `LOG_TAG_AI` enum 值（避免 enum 重排）
- 不清除裝置上 NVS 的 `ai_cfg` 舊資料

## Decisions

### D1: 保留 `ai_provider/` 目錄，僅從 build 移除

**決定**: 從 root `CMakeLists.txt` 的 `EXTRA_COMPONENT_DIRS` 移除 `components/ai_provider`，並從所有依賴方的 `REQUIRES` 清單移除 `ai_provider`。

**理由**: 機制未定，保留代碼比重新撰寫風險小。ESP-IDF CMake 不會編譯未列在 `EXTRA_COMPONENT_DIRS` 的元件，零 binary 影響。

**替代方案**: `git rm -r components/ai_provider` — 拒絕，資訊遺失風險高。

---

### D2: 移除 `ai_interval_min` 從 `schedule_config_t`

**決定**: 從 struct 定義和 NVS 讀寫中移除 `ai_interval_min`，NVS key `ai_ivl` 不再存取。

**理由**: struct 欄位若留著但無 timer 使用，會造成混淆；NVS 舊值殘留不影響功能。

**替代方案**: 保留欄位但不使用 — 增加日後混淆風險，不選。

---

### D3: 保留 `storage_ai_*` 函式

**決定**: `storage.h` 和 `storage.c` 的 AI storage 函式全部保留。

**理由**: portal AI tab 保留，`portal_ai_get_handler` 和 `portal_ai_post_handler` 仍需讀寫 NVS，`wifi_manager.c` 對這些函式的依賴不變。

**替代方案**: 移除 storage 函式並改為 portal 直接讀寫 NVS — 增加耦合，不選。

---

### D4: 移除 `ui_manager_log_ai()`

**決定**: 從 `ui_manager.c` 和 `ui_manager.h` 移除此函式。

**理由**: 唯一呼叫方是 `main.c` 的 AI queue 消費區塊，一起移除後即為 dead code。

---

### D5: screen_info sections 重新編號

**決定**: 移除 AI section 後，原有 4 個 sections 縮為 3 個（DEVICE, NETWORK, STOCKS）。靜態變數 `s_section_lbl_2/3`、`s_content_lbl_2/3` 重新對應。

**理由**: 4 個 label 對 3 個 section 的不匹配會造成 null pointer 風險。

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| NVS `ai_cfg` 繼續被讀寫 | portal AI tab 儲存機制保留，key 存在 NVS 但不被 ai_provider 讀取 |
| ai_provider_reload_local_config 呼叫 | portal_ai_post_handler 目前呼叫此函式；移除 ai_provider 後需一併移除此呼叫，改為只儲存不重載 |
| `scheduler_init` 簽名破壞 | 只有 `main.c` 呼叫，一起修改，無外部依賴 |
| `LOG_TAG_AI` 孤立 enum 值 | 保留不移除，`screen_log.c` 的 case 保留為 dead code，編譯器不報錯 |
| screen_info label 重新編號 | 需同步修改靜態變數宣告、`screen_info_refresh()`、`screen_info_create()` 三處，需仔細檢查 |

## Migration Plan

1. 依以下順序修改（最小化中間 build error）：
   - `app_config.h` → `storage.h` / `storage.c` → `scheduler.h` / `scheduler.c` → `wifi_manager.c` → `screen_info.c` → `ui_manager.h` / `ui_manager.c` → `main.c` → 各 `CMakeLists.txt`
2. `./flash.sh --build-only` 確認無編譯錯誤
3. `./flash.sh --erase` 燒錄（可選，清除舊 NVS 以乾淨狀態啟動）
4. 驗證：開啟 portal，確認 WiFi、AI、Stocks 三個 tab 均可用；AI tab 儲存 key 後 NVS 寫入成功但不觸發分析

**Rollback**: `git revert`；`ai_provider/` 目錄完整保留，恢復 CMakeLists.txt 即可重新編譯。

## Open Questions

- 無（此 change 範圍明確，移除只、不新增）
