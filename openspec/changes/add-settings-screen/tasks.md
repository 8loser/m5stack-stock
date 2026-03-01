## 1. ui_manager.h — enum 更新

- [ ] 1.1 在 `screen_id_t` 加入 `SCREEN_SETTINGS = 4`（位於 SCREEN_INFO 之後、SCREEN_COUNT 之前）
- [ ] 1.2 加入 `screen_settings_create()` 與 `screen_settings_load()` 前向宣告（extern）

## 2. screen_settings.c — 頁面建立

- [ ] 2.1 建立 `components/ui/screens/screen_settings.c`，實作 `screen_settings_create()`
- [ ] 2.2 佈局：Title "Settings"（y=22）、roller label（y=50）、lv_roller 5 選項（y=68, 寬200）、checkbox label（y=148）、lv_checkbox（y=168）
- [ ] 2.3 佈局：底部 "Cancel"（灰色）與 "Save"（綠色）兩個觸控按鈕（y=200，各寬120）
- [ ] 2.4 Cancel 按鈕 event handler：呼叫 `ui_manager_switch_screen(SCREEN_PORTAL)`，不儲存

## 3. screen_settings.c — 載入與儲存邏輯

- [ ] 3.1 實作 `screen_settings_load()`：呼叫 `scheduler_get_config()`，將 `quote_interval_s` 映射至 roller index（fallback index=1），設定 checkbox 核取狀態
- [ ] 3.2 定義 `static const uint16_t INTERVAL_VALUES[] = {30, 60, 120, 300, 600}` 與對應的 roller options string
- [ ] 3.3 Save 按鈕 event handler：從 roller 取 index → 查 INTERVAL_VALUES；從 checkbox 取 bool；呼叫 `scheduler_get_config()` 保留 ai_interval_min；`scheduler_apply_config()`；`storage_schedule_save()`；`ui_manager_switch_screen(SCREEN_PORTAL)`

## 4. ui_manager.c — 初始化與 switch_screen 整合

- [ ] 4.1 在 `ui_manager_init()` 加入 `screen_settings_create()` 呼叫，結果存入 `s_screens[SCREEN_SETTINGS]`
- [ ] 4.2 在 `ui_manager_switch_screen()` 的 mutex 持有段新增 `if (id == SCREEN_SETTINGS)` 分支，呼叫 `screen_settings_load()`
- [ ] 4.3 確認 `s_screens[SCREEN_COUNT]` 陣列大小隨 SCREEN_COUNT=5 自動正確

## 5. screen_portal.c — Settings 入口按鈕

- [ ] 5.1 在右側面板（`s_qr_area` 右側資訊區）底部新增 "Settings" lv_btn
- [ ] 5.2 按鈕 event handler：`LV_EVENT_CLICKED` → `ui_manager_switch_screen(SCREEN_SETTINGS)`

## 6. CMakeLists.txt — 加入 source 檔

- [ ] 6.1 在 `components/ui/CMakeLists.txt` SRCS 加入 `"screens/screen_settings.c"`

## 7. 驗證

- [ ] 7.1 Portal 頁面可看到 "Settings" 按鈕，按下後進入 Settings 頁面
- [ ] 7.2 進入 Settings 時 roller 與 checkbox 反映當前 scheduler 設定
- [ ] 7.3 調整 roller 為 "5 min" 後按 Save，重新進入 Settings 確認 roller 仍選 "5 min"
- [ ] 7.4 調整後按 Cancel，重新進入 Settings 確認設定未變
- [ ] 7.5 硬體 btn=1（中鍵）在 Settings 頁面仍正常觸發 Portal toggle，不觸發 Save
