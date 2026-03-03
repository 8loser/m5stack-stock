## Context

Dashboard 共有 5 個股票卡片列，每張卡片以手動佈局方式呈現 4 欄資訊。目前欄位順序為：

```
Name (symbol+name) | Industry | Price | Change%
     col[0]            col[1]   col[2]  col[3]
```

欄寬由 `compute_card_layout()` 計算，目前採四欄等寬分配（每欄約 70px）。
Widget 建立位置在 `ensure_card_widgets()` 中以 `lv_obj_set_pos()` 硬編碼對應欄位。

唯一受影響檔案：`components/ui/screens/screen_dashboard.c`

## Goals / Non-Goals

**Goals:**
- 將 Industry 移至第四欄（col[3]），Price 移至 col[1]，Change% 移至 col[2]
- 依內容特性調整欄寬比例，讓各欄空間更符合實際內容長度

**Non-Goals:**
- 不新增欄位或 widget 類型
- 不調整卡片高度、字型、顏色
- 不影響資料流、快取、輪播邏輯

## Decisions

### 欄位順序：Name → Price → Change% → Industry

**理由**：財務關鍵資料（價格、漲跌）優先顯示，產業別作為輔助資訊置於最後，符合使用者視線從左到右的優先順序。

### 欄寬分配：非等寬

**新分配（content_w = 278px）：**

| 欄 | 內容 | 寬度 | 理由 |
|----|------|------|------|
| col[0] | Name (symbol\nname) | 85px | 兩行文字，需較多空間 |
| col[1] | Price | 68px | 右對齊數字，最大 "9999.99" 7 chars |
| col[2] | Change% | 55px | 右對齊，"+99.99%" 7 chars，字型較小 |
| col[3] | Industry | 70px | remainder，LONG_DOT 截斷 |

**替代方案考慮**：維持等寬（70px），捨棄理由是 Name 欄兩行文字需要稍多空間，Change% 欄數字短不需要 70px。

### 實作方式：只改 pos/size，不重構 widget 陣列

Widget 陣列（`s_name_labels`、`s_price_labels` 等）維持原本命名與語義，只更新 `lv_obj_set_pos()` 和 `lv_obj_set_size()` 傳入的欄位索引，邏輯最小化。

## Risks / Trade-offs

- **字型截斷**：Industry 欄位 70px 空間對於較長的產業名稱（如「電子零組件業」）仍可能截斷 → 已設定 `LV_LABEL_LONG_DOT`，可接受
- **Name 欄 85px**：多數股票名稱為 3–4 個中文字，14px 字型約 56px，空間充裕；若未來有更長名稱，`LV_LABEL_LONG_CLIP` 仍可保護

## Migration Plan

1. 修改 `compute_card_layout()`：調整 `w_name / w_price / w_change / w_industry` 數值與 `s_col_w[]` 對應
2. 修改 `ensure_card_widgets()`：將各 label 的 `lv_obj_set_pos()` 參數改用新欄位索引
3. Build & flash 驗證顯示效果

Rollback：git revert 該 commit 即可恢復原始欄位順序。

## Open Questions

（無）
