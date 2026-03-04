# AI Prompt 可落地性評估（對應 `temp.txt`）

## 文件目的

本文件評估 `temp.txt` 的需求在目前 `m5stack_stock` 專案中的實作可行性，分為：

- 可直接落地（高可行）
- 需降規後可做（中可行）
- 目前不建議直接做（低可行）

評估基準為目前程式碼現況，不含未實作中的外部整合。

## 可直接落地（高可行）

### 1. TWSE 自選股每 60 秒監控

- 現況已具備排程抓價流程，且預設輪詢間隔為 60 秒。
- 可在執行期調整 interval，並支援手動立即觸發一次抓價。

參考：
- `include/app_config.h` (`DEFAULT_QUOTE_INTERVAL_S`)
- `components/scheduler/scheduler.c` (`do_fetch_quotes`, `scheduler_apply_config`, `scheduler_trigger_quote_now`)

### 2. 台股報價基本欄位分析

- 已解析現價、漲跌幅、昨收、開高低、成交量、交易時間、漲跌停價。
- 已處理休市快照與 `"-"` 欄位回傳情境。

參考：
- `components/twse_client/include/twse_models.h`
- `components/twse_client/twse_client.c` (`parse_stock_item`)
- `docs/twse_api_fields.md`

### 3. AI 建議輸出（buy/sell/hold）

- 已有 OpenAI / Gemini / Claude provider。
- 已定義統一輸出：`signal`、`confidence`、`analysis`。
- 已有內建 prompt template，使用即時 quote 內容組裝分析上下文。

參考：
- `components/ai_provider/include/ai_provider.h`
- `components/ai_provider/ai_provider.c`
- `components/ai_provider/providers/openai.c`
- `components/ai_provider/providers/gemini.c`
- `components/ai_provider/providers/claude.c`

### 4. 自選股清單管理（台股上市）

- Portal API 已支援新增/刪除股票與驗證代號。
- 新增代號時限制 4 碼，且需為 `tse` 市場。
- 新增後可觸發重載清單與立即抓價。

參考：
- `components/portal_backend/stock_admin_service.c` (`is_symbol_format_valid`, `portal_stocks_add_post_handler`)
- `components/twse_client/twse_client.c` (`twse_client_validate_symbol`)
- `include/app_config.h` (`MAX_STOCK_COUNT`)

## 需降規後可做（中可行）

### 1. 盤前/盤中/盤後三段摘要

- 可基於現有 TWSE 報價與 AI provider 生成「摘要型輸出」。
- 需降規為「單一資料源（TWSE + 本機規則 + AI）」。
- 不含跨平台研究報告/社群情緒自動彙整。

### 2. 規則型監控告警（不含 Telegram）

- 可先做本機規則判斷與 UI/Log 告警，例如：
  - 單位時間漲跌幅超過門檻
  - 成交量異常（相對近幾輪平均）
- 需暫不納入以下指標：
  - VWAP
  - 外盤/內盤成交比
  - 完整委買委賣深度推導
  - 5 日每分鐘歷史均量（目前無歷史分鐘資料管線）

### 3. 時間策略調整

- 目前是 interval 輪詢 + `market_only` 市場時段判斷。
- 若需求是精準 08:40、08:55 任務，需新增 cron-like 定時層。
- 現階段建議改寫為「盤前區間」與「盤中固定頻率」描述。

參考：
- `components/scheduler/scheduler.c`
- `include/app_config.h` (`MARKET_OPEN_HOUR`, `MARKET_CLOSE_HOUR`)

## 目前不建議直接做（低可行）

### 1. 多資料源即時整合

下列來源目前專案無整合模組與資料管線：

- Yahoo 股市
- MOPS
- Goodinfo
- 財報狗
- MorningStar
- TipRanks
- Threads 達人觀點

### 2. 美股監控

- 現有 symbol 流程與驗證聚焦台股上市（TSE）。
- 若要支援美股，需新增市場模型、symbol 規則、資料來源與 UI 呈現策略。

### 3. 完整交易微結構策略

以下策略要件目前缺乏可直接計算的完整原始資料：

- 委買/委賣前五檔量比即時推導
- 外盤成交比（Execution Probability）
- VWAP 跌破判斷
- 大盤分時均線跌破判斷

### 4. 真正持股損益結算報告

- 目前僅有「追蹤股票清單」，沒有完整持倉資料模型（股數、成本、分批買入、已實現損益）。
- 若要做盤後 PnL 報告，需先新增 portfolio/position storage schema。

## 建議版 Prompt 範圍（v1）

建議先將 `temp.txt` 改寫為以下範圍：

- 市場範圍：台股上市（TSE）自選股，最多 15 檔
- 更新頻率：每 60 秒（可調）
- 輸出：盤前摘要、盤中監控摘要、盤後摘要
- 分析：基本價量 + AI 建議（buy/sell/hold）
- 告警：先做本機規則與 UI/Log 提示（不含 Telegram）

延後到 v2/v3 再處理：

- Telegram 推播
- 多來源研究/新聞/社群整合
- 美股
- 持倉成本與損益帳務模型

## 建議里程碑

### M1：現有能力對齊 Prompt

- 將 prompt 語句收斂至 TWSE + 現有 AI provider + 本機排程能力。

### M2：補強規則引擎與摘要模板

- 新增可配置門檻與三段式摘要輸出模板。

### M3：外部整合與策略擴張

- 加入 Telegram，再評估多資料源與跨市場（美股）擴充。
