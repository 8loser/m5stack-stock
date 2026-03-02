## 1. 確認前置條件

- [x] 1.1 讀取 `components/storage/storage.c` 確認 `storage_schedule_load()` 預設值行為（NVS 未初始化時返回 `DEFAULT_MARKET_ONLY = true`）
- [x] 1.2 讀取 `include/app_config.h` 確認 `DEFAULT_MARKET_ONLY` 定義

## 2. 修正 market_only 邏輯（主要修正）

- [x] 2.1 修改 `components/scheduler/scheduler.c` 的 `do_fetch_quotes()`：將 `if (!rtc_bm8563_is_market_open())` 改為 `if (s_config.market_only && !rtc_bm8563_is_market_open())`
- [x] 2.2 修改 `include/app_config.h`：將 `DEFAULT_MARKET_ONLY` 從 `true` 改為 `false`，讓首次使用者預設全天候顯示昨收資料

## 3. 修正 SNTP 競爭條件

- [x] 3.1 修改 `components/scheduler/scheduler.c` 的 `do_fetch_quotes()`：在市場時段判斷前加入 `if (!s_sntp_synced) goto do_fetch;` 或等效邏輯（當 SNTP 未同步時跳過時段限制）

## 4. 修正 UI 字元顯示問題

- [x] 4.1 修改 `components/ui/screens/screen_dashboard.c`：將初始佔位符 `"±0.00%"` 改為 `"+/-"`（純 ASCII，避免 Montserrat 字型缺字）

## 5. 驗證修正

- [x] 5.1 Build 確認無編譯錯誤：`./flash.sh --build-only`
- [x] 5.2 Flash 並觀察 monitor log：確認 `do_fetch_quotes()` 在非交易時間不再輸出「非市場時段，跳過報價抓取」
- [x] 5.3 確認 Dashboard 卡片顯示昨收資料（`is_market_closed = true`），而非佔位符 `---`
- [x] 5.4 確認 `+/-` 佔位符正常渲染，無方框 `[]` 出現
