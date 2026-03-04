## Context

目前 `telegram_bot` 在 `/info` 命令處理流程中直接呼叫 `twse_client_fetch()`，造成指令執行具有外網依賴，且 message polling 使用記憶體內 `s_next_update_id`，重開機後會回到 0，可能拉到歷史 backlog。此變更跨 `main` 與 `telegram_bot` 兩個模組，需要先固定資料來源、同步流程與丟棄策略。

## Goals / Non-Goals

**Goals:**
- `/info` 回覆只使用 Core2 本機可得資料，不在命令路徑做 TWSE fetch
- 建立 thread-safe RAM quote 快取，供 Telegram 回覆使用
- 啟動 Telegram task 時跳過開機前 backlog，只處理開機後訊息
- 保留既有 command routing 與 bot 啟停架構

**Non-Goals:**
- 不新增 quote 持久化（重開機後無 quote 時顯示 N/A）
- 不改 scheduler 抓價頻率、TWSE client parser、UI 更新流程
- 不改 Telegram 指令集合（仍是 `/info`、`/help`）

## Decisions

1. `/info` 只讀本機資料，不呼叫 TWSE
- Why: 指令延遲與可用性不再受外網波動影響，並符合「回傳 Core2 內儲存資料」需求。
- Alternative: 保留即時 fetch 並加 fallback；缺點是仍有外網依賴與不確定性。

2. quote 快取採 RAM + mutex，更新點放在 `main` queue 消費後
- Why: 主迴圈已是單一 quote 匯流點，掛載成本最低且不需改 scheduler 事件定義。
- Alternative: 在 scheduler 或 twse_client 端直接寫入；缺點是耦合更高，且與 UI 消費路徑分離。

3. backlog 丟棄採啟動 bootstrap sync
- Why: 啟動時先讀 `getUpdates` 並把 offset 推到最新+1，可確保開機前訊息全忽略。
- Alternative: 只做 message timestamp TTL；缺點是仍要下載並解析 backlog，且邊界較複雜。

4. stale 判定固定 30 分鐘
- Why: 與台股盤中更新頻率相比可合理標示資料新鮮度，且配置簡單。
- Alternative: 可配置門檻；缺點是需要額外設定 API/UI，超出本次範圍。

## Risks / Trade-offs

- [重開機後尚未抓到新報價] → `/info` 會顯示 N/A；在訊息中顯示 `Last quote update: N/A` 降低誤解
- [telegram task 與 main loop 併發讀寫快取] → 以 mutex 保護快取 map 與時間戳
- [bootstrap sync 失敗時行為不明] → 失敗則延後重試，不進入命令處理路徑
- [訊息長度增加] → 延用既有 `append_fmt` 安全截斷機制與 3072 bytes response buffer

## Migration Plan

1. 新增 Telegram quote 快取 API 與內部資料結構（先不改 `/info`）
2. 在 `main` 收到 quote 後寫入快取，確認不影響 UI 更新
3. 改寫 `/info` 字串組裝為本機來源，移除 `twse_client_fetch` 路徑
4. 新增 telegram task 啟動 bootstrap sync，啟用 backlog 丟棄
5. 執行 build 與實機命令流程驗證（含開機前訊息忽略）

## Open Questions

- 無（本次規格已定案：本機回覆 + 啟動丟棄 backlog）
