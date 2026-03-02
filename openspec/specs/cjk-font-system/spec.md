# cjk-font-system Specification

## Purpose
為 UI 提供可維護的繁體中文字型子集機制，確保中文字可正確顯示並控制韌體體積。

## Requirements
### Requirement: 使用 lv_font_conv 產生 Noto Sans TC 子集字型
系統 SHALL 使用 `lv_font_conv --no-compress --no-prefilter --bpp 4 --format lvgl` 從 Noto Sans TC TTF 產生子集字型 .c 檔，包含 ASCII 0x20-0x7F 及 UI 所需的繁體中文字元。

#### Scenario: 產生 14px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 14`
- **THEN** 產生 `lv_font_noto_tc_14.c`，包含所有 UI 所需字元，無壓縮

#### Scenario: 產生 16px 字型
- **WHEN** 執行字型產生腳本，指定 `--size 16`
- **THEN** 產生 `lv_font_noto_tc_16.c`，包含所有 UI 所需字元，無壓縮

### Requirement: 字型檔案放置於 components/ui/fonts/
子集字型 .c 檔 SHALL 放置於 `components/ui/fonts/` 目錄，並在 `components/ui/CMakeLists.txt` 的 SRCS 中列出。

#### Scenario: Build 包含字型檔案
- **WHEN** 執行 `idf.py build`
- **THEN** `lv_font_noto_tc_14.c` 和 `lv_font_noto_tc_16.c` 被編譯進韌體

### Requirement: UI_FONT_TEXT_DEFAULT 為 Noto Sans TC 14px
`ui_compat.h` 中 `UI_FONT_TEXT_DEFAULT` SHALL 定義為 `lv_font_noto_tc_14`，作為所有 UI 文字的預設字型。

#### Scenario: 預設字型為 Noto Sans TC 14px
- **WHEN** 任一 UI 元件使用 `UI_FONT_TEXT_DEFAULT`
- **THEN** 該元件的字型為 `lv_font_noto_tc_14`（14px，含 ASCII + 繁中子集）

### Requirement: ui_font.h 提供字型 extern 宣告
`components/ui/include/ui_font.h` SHALL 提供 `lv_font_noto_tc_14` 和 `lv_font_noto_tc_16` 的 extern 宣告，供各 screen 檔案引用。

#### Scenario: Screen 檔案引用字型
- **WHEN** screen_dashboard.c include `ui_font.h`
- **THEN** 可使用 `&lv_font_noto_tc_14` 和 `&lv_font_noto_tc_16`

### Requirement: 字型產生腳本
專案 SHALL 包含字型產生腳本（shell script），方便未來新增字元時重新產生字型。腳本 SHALL 記錄所有 `--symbols` 字元。

#### Scenario: 新增 UI 字元
- **WHEN** 開發者需要新增 UI 中文字
- **THEN** 修改腳本的 `--symbols` 參數後執行，即可重新產生字型 .c 檔
