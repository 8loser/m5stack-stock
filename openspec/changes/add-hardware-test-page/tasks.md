## 1. ui_manager.h — enum 更新

- [ ] 1.1 在 `screen_id_t` 加入 `SCREEN_HW_TEST`（位於現有最後一個頁面 enum 之後、SCREEN_COUNT 之前）

## 2. screen_hw_test.c — 頁面建立

- [ ] 2.1 建立 `components/ui/screens/screen_hw_test.c`，實作 `screen_hw_test_create()`
- [ ] 2.2 建立頁面背景（深色）、標題 "HW TEST"（y=22, montserrat_20, 置中）
- [ ] 2.3 建立 "VIBRATION" section label（y=44, 顏色 0x4FC3F7, montserrat_12）
- [ ] 2.4 建立 Vibration 區塊 3 個按鈕（y=60, h=36）：[Haptic] [Alert] [Pulse 500ms]，各寬 95px，間距 5px
- [ ] 2.5 建立 "AUDIO" section label（y=104, 顏色 0xCE93D8, montserrat_12）
- [ ] 2.6 建立 Audio 區塊第一行 2 個按鈕（y=120, h=36）：[Beep 1kHz] [Alert Up]，各寬 147px，間距 6px
- [ ] 2.7 建立 Audio 區塊第二行 2 個按鈕（y=162, h=36）：[Alert Down] [Notify]，各寬 147px，間距 6px

## 3. screen_hw_test.c — 按鈕事件 Handler

- [ ] 3.1 Haptic 按鈕 `LV_EVENT_CLICKED`：呼叫 `vibration_haptic()`
- [ ] 3.2 Alert 按鈕 `LV_EVENT_CLICKED`：呼叫 `vibration_alert()`
- [ ] 3.3 Pulse 500ms 按鈕 `LV_EVENT_CLICKED`：呼叫 `vibration_pulse(500)`
- [ ] 3.4 Beep 1kHz 按鈕 `LV_EVENT_CLICKED`：呼叫 `audio_beep(1000, 200)`
- [ ] 3.5 Alert Up 按鈕 `LV_EVENT_CLICKED`：呼叫 `audio_alert_up()`
- [ ] 3.6 Alert Down 按鈕 `LV_EVENT_CLICKED`：呼叫 `audio_alert_down()`
- [ ] 3.7 Notify 按鈕 `LV_EVENT_CLICKED`：呼叫 `audio_notify()`

## 4. ui_manager.c — 初始化整合

- [ ] 4.1 在 `ui_manager_init()` 加入 `screen_hw_test_create()` 呼叫，結果存入 `s_screens[SCREEN_HW_TEST]`
- [ ] 4.2 加入 `extern lv_obj_t *screen_hw_test_create(void);` 前向宣告

## 5. screen_portal.c — HW Test 入口按鈕

- [ ] 5.1 在右側面板新增 "HW Test" lv_btn（與 Settings 按鈕合理排列，不遮蔽 QR 區）
- [ ] 5.2 按鈕 event handler：`LV_EVENT_CLICKED` → `ui_manager_switch_screen(SCREEN_HW_TEST)`

## 6. CMakeLists.txt — 加入 source 檔

- [ ] 6.1 在 `components/ui/CMakeLists.txt` SRCS 加入 `"screens/screen_hw_test.c"`

## 7. 驗證

- [ ] 7.1 Portal 頁面可看到 "HW Test" 按鈕，按下後進入 HW Test 頁面
- [ ] 7.2 Haptic 按鈕按下有短震動
- [ ] 7.3 Alert 按鈕按下有長震動
- [ ] 7.4 Pulse 500ms 按鈕按下有明顯較長震動
- [ ] 7.5 Beep 1kHz 按鈕按下喇叭發出短蜂鳴
- [ ] 7.6 Alert Up / Alert Down 按鈕各發出對應音調
- [ ] 7.7 Notify 按鈕發出提示音
- [ ] 7.8 HW Test 頁面按 btn=1（中鍵）正常回到 Portal（全域語義不受影響）
