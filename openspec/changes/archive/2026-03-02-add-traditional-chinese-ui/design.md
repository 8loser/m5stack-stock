## Context

UI 目前全英文（Montserrat 字型）。先前嘗試 `lv_font_conv` 產生繁中子集字型失敗，原因為 RLE 壓縮 edge case。社群研究顯示使用 `--no-compress --no-prefilter` 可避開此問題。

LVGL 內建 `lv_font_simsun_16_cjk` 雖已編譯進韌體，但僅含 1097 字（缺 15 個常用字）、只有 16px、宋體風格老舊，不適合作為主方案。

## Goals / Non-Goals

**Goals:**
- 用 `lv_font_conv --no-compress --no-prefilter` 從 Noto Sans TC 產生子集字型
- 將 UI 所有使用者可見文字翻譯為繁體中文
- 支援多種字型尺寸（至少 14px 和 16px）
- 正確顯示中文，無亂碼

**Non-Goals:**
- 不做多語言切換機制（只需中文）
- 不使用 Tiny TTF 即時渲染（效能差）
- 不使用 binary font + Flash partition（過度複雜）
- 不翻譯技術性英文縮寫（WiFi、SSID、IP、API、AP、QR）

## Decisions

### D1: 字型來源 — Noto Sans TC（思源黑體）

**選擇**: Google Noto Sans Traditional Chinese，OFL 授權，現代黑體風格。

**替代方案**:
- simsun（宋體）→ 風格老舊、只有 1097 字、15 字缺失需繞詞
- 自選其他 TTF → Noto Sans TC 為 Google 官方繁中字型，品質穩定且免費

### D2: 字型產生方式 — lv_font_conv 無壓縮子集

**選擇**: `lv_font_conv --no-compress --no-prefilter --bpp 4 --format lvgl` 產生 .c 檔，只包含 UI 所需字元（~100 字 + ASCII 0x20-0x7F）。

**參數說明**:
- `--no-compress`: 避開 RLE 壓縮 bug（先前失敗原因）
- `--no-prefilter`: 配合 no-compress 使用
- `--bpp 4`: 4-bit 抗鋸齒，品質與大小平衡
- `--symbols "所需中文字..."`: 只包含 UI 用到的字

**替代方案**:
- 含壓縮 → 先前已證實有 bug，不可靠
- `--bpp 2` → 品質較差，省不了多少空間
- `--format bin` → 需要額外 Flash partition 設定，過度複雜

### D3: 字型尺寸策略

產生兩種尺寸的子集字型：

| 尺寸 | 用途 | 檔案名 |
|------|------|--------|
| 14px | 一般文字、狀態列、小標籤 | `lv_font_noto_tc_14.c` |
| 16px | 標題、重要文字 | `lv_font_noto_tc_16.c` |

**數字/符號**: Noto Sans TC 包含完整 ASCII，無需額外保留 Montserrat。可移除 Montserrat 依賴。

**替代方案**:
- 只用 16px → 失去層次感
- 加入 10px/20px → UI 空間有限，兩種尺寸已足夠

### D4: 字型檔案位置與 Build 整合

```
components/ui/
  fonts/
    lv_font_noto_tc_14.c
    lv_font_noto_tc_16.c
  include/
    ui_font.h          # extern 宣告
    ui_compat.h         # UI_FONT_TEXT_DEFAULT 改為 lv_font_noto_tc_14
```

`CMakeLists.txt` 的 SRCS 加入 `fonts/lv_font_noto_tc_14.c` 和 `fonts/lv_font_noto_tc_16.c`。

### D5: 翻譯策略 — 自然繁中用詞

因為自製字型可包含任意字元，不再受 simsun 缺字限制。翻譯可使用最自然的繁體中文用詞：

| 英文 | 中文 |
|------|------|
| Dashboard | 儀表板 |
| Log | 日誌 |
| Info | 資訊 |
| Settings | 設定 |
| Portal | 入口設定 |
| Quote Interval | 報價間隔 |
| 1/5/10 min | 1/5/10 分鐘 |
| Saved successfully | 儲存成功 |
| Save failed | 儲存失敗 |
| DEVICE | 裝置 |
| NETWORK | 網路 |
| STOCKS | 股票 |
| Connected | 已連線 |
| Connecting | 連線中 |
| Offline | 離線 |
| closed | 休市 |
| Heap | 記憶體 |
| Chip | 晶片 |
| Event Log | 事件日誌 |

### D6: 字型產生腳本

提供一個 shell script 或 Makefile target，方便未來新增字元時重新產生字型：

```bash
npx lv_font_conv \
  --no-compress --no-prefilter \
  --bpp 4 --size 14 \
  --font NotoSansTC-Regular.ttf \
  -r 0x20-0x7F \
  --symbols "儀表板日誌資訊設定入口報價間隔分鐘儲存成功失敗裝置網路股票已連線中離未晶片記憶體事件系統供應商密碼金鑰可用空間型式更新時休市電量掃描加啟動查看提交通行結搜尋狀態保開關注清單" \
  --format lvgl \
  -o lv_font_noto_tc_14.c \
  --force-fast-kern-format
```

## Risks / Trade-offs

| 風險 | 緩解 |
|------|------|
| `--no-compress` 仍可能有 bug | 先產生字型並測試編譯+顯示，確認無問題再全面翻譯 |
| Noto Sans TC TTF 檔很大（~15MB） | 只在開發機上需要，不進 git；產生的 .c 檔很小（~30-50KB/尺寸） |
| 未來新增 UI 文字需重新產生字型 | 提供字型產生腳本，新增字元後重跑即可 |
| 子集字型 .c 檔佔 Flash 空間 | 80 字 + ASCII 的 4bpp 字型約 30-50KB/尺寸，兩個尺寸約 60-100KB，可接受 |
| lv_font_conv 需要 Node.js | 開發者環境依賴，非運行時依賴；可在 CI 或本地執行 |
