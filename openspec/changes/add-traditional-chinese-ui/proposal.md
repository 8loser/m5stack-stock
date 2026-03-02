## Why

UI 目前全英文，使用者為台灣市場。繁體中文介面可提升使用體驗與可讀性。先前嘗試 `lv_font_conv` 子集字型因 RLE 壓縮 bug 失敗，但社群證實使用 `--no-compress --no-prefilter` 參數可避開此問題。改用 Noto Sans TC（思源黑體）產生自訂子集字型，可完整支援所需繁中字元。

## What Changes

- 使用 `lv_font_conv` 從 Noto Sans TC 產生只含 UI 所需字元的子集字型 .c 檔（`--no-compress --no-prefilter`）
- 將所有 UI 可見文字從英文翻譯為繁體中文
- 將 label 字型從 Montserrat 改為自製 Noto Sans TC 子集字型
- 更新 `ui_compat.h` 的 `UI_FONT_TEXT_DEFAULT` 為新字型
- **BREAKING**: UI 語言從英文改為繁體中文

## Capabilities

### New Capabilities
- `cjk-font-system`: 建立 CJK 字型產生與使用機制（lv_font_conv 子集字型、Noto Sans TC、多尺寸支援）

### Modified Capabilities
- `ui-manager`: UI 文字內容從英文改為繁體中文，字型從 Montserrat 改為 Noto Sans TC 子集

## Impact

- **受影響檔案**: `components/ui/` 下所有 screen 檔案（dashboard、settings、info、portal、log）及 `widgets/status_bar.c`
- **新增檔案**: `components/ui/fonts/` 目錄下的自製字型 .c 檔
- **工具依賴**: 需要 Node.js + `lv_font_conv` CLI 工具、Noto Sans TC TTF 字型檔
- **字型**: `ui_compat.h` 字型定義需重寫
- **Build 設定**: `components/ui/CMakeLists.txt` 需加入字型 .c 檔
- **Flash 大小**: 子集字型約 80 字 + ASCII，預估每個尺寸 ~30-50KB
