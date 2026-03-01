## Context

Settings 頁面目標改為最小交互：
- 只調整 `quote_interval_s`
- 只提供三個 interval 選項（1/5/10 分）
- 點擊即儲存，不再需要 Save/Cancel
- 非開市不抓價改為固定策略，不在 UI 暴露開關

現有基礎：
- `ui_manager` 已可建立與切換多頁，且底部虛擬按鍵會攔截 `y >= 200` 觸控
- `scheduler_get_config()` / `scheduler_apply_config()` 可讀寫執行中排程
- `storage_schedule_save()` 可持久化設定

## Goals / Non-Goals

**Goals**
- Core2 可透過頁面輪詢進入 `SCREEN_SETTINGS`
- 三個 interval 按鈕（1/5/10 min）可立即儲存
- 顯示儲存成功/失敗訊息
- 非開市固定不抓報價

**Non-Goals**
- 不提供 `market_only` UI 開關
- 不提供 Save/Cancel 按鈕
- 不調整 `ai_interval_min`

## Decisions

### D1：入口與導航
`SCREEN_SETTINGS` 納入 `s_nav_screens[]`，沿用 Core2 硬體按鍵輪詢。

### D2：三鍵即時儲存
`INTERVAL_VALUES = {60, 300, 600}`。點擊任一按鈕後立即執行：
1. `scheduler_get_config(&cfg)` 取得現值
2. 設定 `cfg.quote_interval_s`
3. 強制 `cfg.market_only = true`
4. `scheduler_apply_config(&cfg)` + `storage_schedule_save(&cfg)`
5. 於頁面顯示 `Saved successfully` 或 `Save failed`

### D3：非開市固定跳過抓價
`scheduler.c` 的 `do_fetch_quotes()` 直接以 `rtc_bm8563_is_market_open()` 決定是否抓取，非開市一律跳過。

### D4：版面與觸控安全區
所有可點擊元件維持 `y < 200`，避免被底部虛擬硬體按鍵攔截。

## Risks / Trade-offs

| 風險 | 緩解方式 |
|------|---------|
| 由五檔縮減為三檔 interval，粒度變粗 | 維持常用選項 1/5/10 min，簡化操作 |
| 立即儲存增加誤觸風險 | 按鈕高亮 + 儲存結果訊息降低誤判 |
| 固定非開市不抓價缺少彈性 | 符合目前需求，後續可再擴充 |

## Migration Plan

1. `screen_settings.c`：改為三鍵即時儲存，移除 checkbox/Save/Cancel
2. `scheduler.c`：固定非開市不抓價
3. 更新 OpenSpec tasks/specs 與驗證項目
