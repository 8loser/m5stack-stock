## Context

目前 `handle_hw_button()` 以 `switch (s_cur_screen)` per-screen 分支處理：
- `SCREEN_DASHBOARD`: btn1 → 呼叫 `screen_dashboard_on_btn(1)` 刷新報價，btn2 → Portal
- `SCREEN_PORTAL`: btn0 → 回到 Dashboard
- 其他頁面（Log / Info / Settings）：無按鍵行為

`screen_id_t` 目前只有 `SCREEN_DASHBOARD=0`、`SCREEN_PORTAL=1`。
每新增一個頁面就必須修改 switch-case，且語義不一致。

## Goals / Non-Goals

**Goals:**
- 統一三鍵語義：中間 = Portal toggle，左右 = 主頁面輪詢
- 新增頁面只需把 ID 加入 `s_nav_screens[]`，不修改 switch-case
- Portal 維持「快捷入口」語義（不加入輪詢清單）
- 保留 `ui_manager_switch_screen()` 對外 API 不變

**Non-Goals:**
- 不改變 Portal 頁面的觸控 UI 互動
- 不為 Dashboard 刷新功能提供替代入口（由觸控 UI 處理）
- 不支援長按或組合鍵

## Decisions

### D1：引入 s_nav_screens[] 可輪詢清單

```c
static const screen_id_t s_nav_screens[] = {
    SCREEN_DASHBOARD,
    SCREEN_LOG,
    SCREEN_INFO,
    // SCREEN_SETTINGS 暫不加入（功能性頁面，非主內容流）
};
#define NAV_SCREENS_COUNT (sizeof(s_nav_screens) / sizeof(s_nav_screens[0]))
static int s_nav_idx = 0;  // 目前在清單中的位置
```

替代方案：動態 array + register API。選擇靜態清單因為頁面集合在編譯期已知，簡單可靠。

### D2：Portal 返回目標固定為 Dashboard

不追蹤 `s_prev_screen`。在 `SCREEN_PORTAL` 中按任一按鍵（btn0/1/2）都固定回 `SCREEN_DASHBOARD`。
此設計簡化心智模型，也符合目前產品需求。

### D3：移除 per-screen switch-case，改用全域邏輯

```
handle_hw_button(btn):
  if s_cur_screen == SCREEN_PORTAL:
    if btn <= 2: switch_screen(SCREEN_DASHBOARD)
  else:
    if btn == 1: switch_screen(SCREEN_PORTAL)
    if btn == 0: s_nav_idx = (s_nav_idx - 1 + NAV_SCREENS_COUNT) % NAV_SCREENS_COUNT
                 switch_screen(s_nav_screens[s_nav_idx])
    if btn == 2: s_nav_idx = (s_nav_idx + 1) % NAV_SCREENS_COUNT
                 switch_screen(s_nav_screens[s_nav_idx])
```

### D4：移除 screen_dashboard_on_btn(1) 對按鍵的依賴

刷新報價功能保留在 `screen_dashboard.c` 但只由觸控 UI 觸發；`screen_dashboard_on_btn()` 可整體移除或保留供未來擴充（建議移除以減少死程式碼）。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| 新頁面忘記加入 s_nav_screens[] | 在 CLAUDE.md Gotchas 補充說明；清單位於單一位置，易找到 |
| s_nav_idx 與實際頁面不同步（外部呼叫 switch_screen）| 輪詢時用 `s_nav_screens[s_nav_idx]` 而非 `s_cur_screen`，僅 btn 0/2 才更新 idx，語義正確 |
| Portal 無法返回進入前頁面 | 這是需求決策：Portal 作為快捷入口，出口固定回 Dashboard |

## Migration Plan

1. 更新 `screen_id_t`（ui_manager.h）加入未來頁面 enum（若尚未加入）
2. 在 `ui_manager.c` 加入 `s_nav_screens[]`、`s_nav_idx`
3. 重寫 `handle_hw_button()` 為全域邏輯
4. 移除 `screen_dashboard_on_btn()` 中的按鍵入口（或整個函數）
5. 驗證：手動在裝置上測試三鍵在各頁面的行為

無資料遷移，NVS 不涉及。回滾：git revert 單一 commit。
