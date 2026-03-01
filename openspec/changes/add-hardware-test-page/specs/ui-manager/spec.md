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
