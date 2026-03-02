## Context

現有 status bar 只有頁面名稱、時間、WiFi 與電量，沒有「主流程健康度」訊號。由於 LVGL task 可獨立存活，僅靠 UI 動畫不足以判斷 `main` 或 `scheduler` 是否卡住。此變更需跨 `main`、`scheduler`、`ui-manager`、`status_bar` 與字型工具鏈，屬於跨模組協作。

## Goals / Non-Goals

**Goals:**
- 提供可目視辨識的主流程活性指標，快速判斷是否疑似當機。
- 以最小 UI 侵入方式整合到共用 status bar，不增加每頁重複邏輯。
- 保留 emoji 視覺需求，同時提供缺字 fallback 確保可顯示。

**Non-Goals:**
- 不新增 watchdog 重啟或任何自動復原機制。
- 不更動既有頁面切換流程、Portal 連線邏輯或報價抓取邏輯本身。
- 不調整 TWSE/網路堆疊。

## Decisions

1. 心跳語意採「主流程存活」而非「UI 存活」。
- 理由：符合「是否當機」需求；可避免 UI 仍動時的假陽性。
- 方案：維護兩個時間戳（`main`、`scheduler`），兩者都在門檻內才判定 alive。

2. 心跳來源由 `main` 與 `scheduler` 主動餵入 `ui_manager`。
- 理由：避免 status bar 直接依賴多模組，保持 UI 單向讀取狀態。
- 介面：`ui_manager_heartbeat_feed_main()`、`ui_manager_heartbeat_feed_scheduler()`、`ui_manager_is_main_flow_alive()`。

3. `scheduler_task` wait timeout 從 60 秒改為 1 秒。
- 理由：若維持 60 秒，活性判斷會長時間無更新，誤報頻繁。
- 影響：增加 task 醒來頻率，但邏輯僅做輕量檢查，成本可接受。

4. status bar 心跳顯示規則。
- 正常：愛心 emoji 雙擊節奏閃爍。
- 異常：`!` 固定紅色且停閃。
- 位置：時間前方，固定寬度避免布局抖動。

5. Emoji 字型策略。
- 先將心跳 emoji 納入 `ui_symbols.txt`，透過既有 `generate_fonts.sh` 產生字型。
- 若渲染失敗（缺字/tofu），runtime fallback 為 `<3`。

## Risks / Trade-offs

- [Risk] Emoji 在目標字型不一定可渲染 → Mitigation: 規範 `<3` fallback，且保留 `!` 異常顯示。
- [Risk] scheduler 每秒喚醒增加少量功耗 → Mitigation: 僅做通知檢查與輕量判斷，不做重工作。
- [Risk] 門檻設定過嚴造成誤判 → Mitigation: main=1500ms、scheduler=3000ms，並在實機監看調整。

## Migration Plan

1. 先落地 `ui_manager` API 與 status bar 顯示，不改既有功能路徑。
2. 在 `main`/`scheduler` 接入餵心跳。
3. 擴充字型符號並重生字型。
4. 以 `./flash.sh --build-only` 與實機 monitor 驗證。
5. 若遇 emoji 顯示問題，先切回 `<3` fallback，不阻擋發布。

## Open Questions

- 無。
