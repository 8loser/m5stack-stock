## ADDED Requirements

### Requirement: SCREEN_HW_TEST enum 新增
`ui_manager.h` SHALL 在 `screen_id_t` enum 中新增 `SCREEN_HW_TEST`，位於現有最後一個頁面 enum 之後、`SCREEN_COUNT` 之前。

#### Scenario: enum 可正常引用
- **WHEN** 程式碼引用 `SCREEN_HW_TEST`
- **THEN** 其值為有效的 screen_id_t，`SCREEN_COUNT` 自動更新為正確總數

### Requirement: screen_hw_test_create 加入初始化
`ui_manager_init()` SHALL 呼叫 `screen_hw_test_create()` 建立 HW Test 頁面物件，並存入 `s_screens[SCREEN_HW_TEST]`。

#### Scenario: 初始化後可切換
- **WHEN** `ui_manager_init()` 完成
- **THEN** `s_screens[SCREEN_HW_TEST]` 非 NULL，`ui_manager_switch_screen(SCREEN_HW_TEST)` 可正常執行

### Requirement: SCREEN_HW_TEST 納入硬體按鍵輪詢導航
`ui_manager.c` 的 `s_nav_screens[]` SHALL 包含 `SCREEN_HW_TEST`，使 Core2 左右鍵輪詢可切換到 HW Test 頁面。

#### Scenario: 左右鍵輪詢包含 HW Test
- **WHEN** 使用者使用 btn=0 或 btn=2 進行頁面輪詢
- **THEN** 輪詢序列包含 `SCREEN_HW_TEST`

### Requirement: status bar 顯示 HW Test 頁面名稱
status bar 頁面名稱對照 SHALL 包含 `SCREEN_HW_TEST -> "HW Test"`。

#### Scenario: 切換至 HW Test
- **WHEN** 呼叫 `ui_manager_switch_screen(SCREEN_HW_TEST)`
- **THEN** status bar 中央顯示文字包含 "HW Test"
