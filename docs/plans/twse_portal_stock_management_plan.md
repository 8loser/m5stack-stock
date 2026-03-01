# TWSE-Only 觀察清單 Portal 管理方案（含 Task 清單）

## Summary
在現有韌體中新增 Portal 股票清單管理，維持 **只支援 TWSE（上市）**。  
使用者可在 Portal 查看目前清單（代號+名稱）、新增、刪除；新增前必須先向 TWSE 驗證。  
資料變更後採 **下個排程週期生效**（不立即抓價）。

## Public API / 介面變更

### 1. `GET /stocks`
- 回傳目前觀察清單。
- Response:

```json
{
  "count": 2,
  "items": [
    {"symbol": "2330", "name": "台積電"},
    {"symbol": "2317", "name": "鴻海"}
  ]
}
```

### 2. `POST /stocks/add`
- Request JSON: `{"symbol":"2330"}`
- 驗證順序：
  1. 格式（4 位數字）
  2. 查重
  3. 上限（10 檔）
  4. TWSE 單檔驗證（且市場別必須 `tse`）
- Success Response:

```json
{
  "ok": true,
  "item": {"symbol": "2330", "name": "台積電", "market": "tse"}
}
```

- Error Code:
  - `invalid_format`
  - `duplicate_symbol`
  - `limit_exceeded`
  - `not_found_or_not_tse`
  - `validate_failed`

### 3. `POST /stocks/remove`
- Request JSON: `{"symbol":"2330"}`
- Success Response:

```json
{"ok": true}
```

- Error Code:
  - `not_found`

### 4. Portal `/` UI
- 新增 Stocks 區塊：
  1. 清單顯示（symbol + name）
  2. 新增輸入框 + Add
  3. 每列 Remove
  4. 成功/錯誤訊息
- 前端呼叫流程：
  1. 初始 `GET /stocks`
  2. 新增 `POST /stocks/add`
  3. 刪除 `POST /stocks/remove`
  4. 成功後刷新清單

## 內部實作規格

### 1. `twse_client` 擴充
- 新增型別：

```c
typedef struct {
    char symbol[8];
    char name[32];
    char market[8];
    bool exists;
} stock_symbol_info_t;
```

- 新增 API：

```c
esp_err_t twse_client_validate_symbol(const char *symbol, stock_symbol_info_t *out);
```

- 驗證條件：
  - API 回傳可解析且有有效代號
  - 市場別必須為 `tse`

### 2. `storage`（維持固定上限 10）
- 沿用 `stock_list_t symbols[10][8]` 與 `count`。
- 新增 helper（可放 `wifi_manager` 私有）：
  - 查重
  - 加入
  - 刪除
- 每次成功異動都呼叫 `storage_stocks_save()`。

### 3. `scheduler`
- 新增 API：

```c
esp_err_t scheduler_reload_stock_list(void);
```

- 行為：
  1. 從 NVS 重新載入 `s_stock_list`
  2. 不立即抓價，等待下個排程週期

### 4. `wifi_manager`
- 註冊新路由：`/stocks`、`/stocks/add`、`/stocks/remove`
- 回應統一 JSON 格式與錯誤碼

## Task Checklist（可直接分派）

1. API 契約與錯誤碼
- 定義 `/stocks*` JSON schema 與錯誤碼常數
- 建立 HTTP status 對應表

2. `twse_client` 驗證能力
- 新增 `stock_symbol_info_t`
- 實作 `twse_client_validate_symbol()`
- 補齊 `market=tse` 檢查與錯誤路徑

3. `storage` 清單操作
- 實作查重/加入/刪除 helper
- 補齊空值、重複、超上限邊界檢查

4. `scheduler` 清單熱重載
- 實作 `scheduler_reload_stock_list()`
- 確認不主動觸發抓價

5. `wifi_manager` 路由與 handlers
- 實作 `GET /stocks`
- 實作 `POST /stocks/add`
- 實作 `POST /stocks/remove`
- 統一 JSON 回應格式

6. Portal 前端
- 新增 Stocks 卡片 UI
- 綁定新增/刪除事件
- 顯示錯誤碼對應訊息

7. Build 與手動驗證
- `./flash.sh --build-only`
- Portal 新增/刪除流程
- `./flash.sh --monitor` 檢查 log 與排程生效時機

8. 文件更新
- README 補充 Portal 股票設定、TWSE-only 限制、10 檔上限

## Test Cases / 驗收情境
1. 新增合法 TWSE 代號成功並顯示名稱
2. 重複代號被拒絕
3. 格式錯誤代號被拒絕
4. OTC/不存在代號被拒絕
5. 第 11 檔被拒絕
6. 刪除存在代號成功
7. 刪除不存在代號回 `not_found`
8. 變更後於下一排程週期才生效
9. TWSE 驗證逾時回 `validate_failed`
10. Build 成功且 monitor 無新增警告

## Assumptions / Defaults
1. 僅支援 TWSE，不接 TPEx
2. 上限維持 10 檔
3. 僅做新增+刪除，不做排序/編輯
4. 新增前必做 TWSE 即時驗證
5. 生效時機為下一排程週期
