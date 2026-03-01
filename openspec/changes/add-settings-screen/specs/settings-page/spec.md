## ADDED Requirements

### Requirement: 輪詢間隔 Roller 選擇
Settings 頁面 SHALL 提供 `lv_roller`，選項為 5 個固定值：30 sec / 1 min / 2 min / 5 min / 10 min，對應秒數 [30, 60, 120, 300, 600]。進入頁面時 SHALL 根據當前 `scheduler_get_config().quote_interval_s` 預選最接近選項。

#### Scenario: 載入當前設定
- **WHEN** 使用者進入 Settings 頁面（`screen_settings_load()` 被呼叫）
- **THEN** roller 自動選中與 `quote_interval_s` 最接近的選項

#### Scenario: 當前值不在選項中
- **WHEN** NVS 儲存的 `quote_interval_s` 不在 [30,60,120,300,600] 中
- **THEN** roller 選中最接近的選項（預設 fallback index=1，即 1 min）

### Requirement: Market Hours Only Checkbox
Settings 頁面 SHALL 提供 `lv_checkbox`，標示 "Market Hours Only"。進入頁面時 SHALL 根據 `scheduler_get_config().market_only` 預設核取狀態。

#### Scenario: 載入 market_only 狀態
- **WHEN** 使用者進入 Settings 頁面
- **THEN** checkbox 核取狀態與 `market_only` 一致（true = checked）

### Requirement: Save 按鈕儲存並返回
Settings 頁面 SHALL 提供 "Save" 觸控按鈕。按下後 SHALL：
1. 以 roller 當前選中的秒數設定 `cfg.quote_interval_s`
2. 以 checkbox 核取狀態設定 `cfg.market_only`
3. 保留其他欄位（`ai_interval_min`）不變
4. 呼叫 `scheduler_apply_config(&cfg)` 使設定即時生效
5. 呼叫 `storage_schedule_save(&cfg)` 持久化到 NVS
6. 呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)` 返回

#### Scenario: 儲存並即時生效
- **WHEN** 使用者調整 roller 為 "5 min" 並按下 Save
- **THEN** `scheduler_apply_config()` 以 300s 更新排程，`storage_schedule_save()` 持久化，畫面切換至 Portal

### Requirement: Cancel 按鈕放棄並返回
Settings 頁面 SHALL 提供 "Cancel" 觸控按鈕。按下後 SHALL 不執行任何儲存，直接呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)` 返回。

#### Scenario: 放棄修改
- **WHEN** 使用者調整 roller 後按下 Cancel
- **THEN** scheduler config 維持原值不變，畫面切換至 Portal

### Requirement: screen_settings_load 進入時載入
每次切換至 `SCREEN_SETTINGS` 時，SHALL 呼叫 `screen_settings_load()`，從 `scheduler_get_config()` 讀取當前值並更新 roller 與 checkbox 狀態，確保顯示最新設定。

#### Scenario: 重新進入頁面
- **WHEN** 使用者從 Portal 進入 Settings 後 Cancel，再次進入 Settings
- **THEN** roller 與 checkbox 重新從 scheduler 載入，反映最新狀態
