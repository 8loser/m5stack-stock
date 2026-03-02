## 1. 字型產生與 Build 設定

- [x] 1.1 下載 Noto Sans TC Regular TTF 字型檔
- [x] 1.2 收集所有 UI 所需繁中字元，建立字型產生腳本 `components/ui/fonts/generate_fonts.sh`
- [x] 1.3 用 `lv_font_conv --no-compress --no-prefilter --bpp 4` 產生 `lv_font_noto_tc_14.c` 和 `lv_font_noto_tc_16.c`
- [x] 1.4 建立 `components/ui/include/ui_font.h`，提供字型 extern 宣告
- [x] 1.5 更新 `components/ui/include/ui_compat.h`：`UI_FONT_TEXT_DEFAULT` 改為 `lv_font_noto_tc_14`
- [x] 1.6 更新 `components/ui/CMakeLists.txt` 加入 `fonts/lv_font_noto_tc_14.c` 和 `fonts/lv_font_noto_tc_16.c`
- [x] 1.7 執行 `./flash.sh --build-only` 確認字型編譯通過

## 2. Status Bar 翻譯

- [x] 2.1 更新 `components/ui/widgets/status_bar.c`：頁面名稱改為中文（儀表板/日誌/資訊/設定/入口設定）
- [x] 2.2 更新 `status_bar.c`：Portal 狀態文字改為中文（已連線/連線中/離線）
- [x] 2.3 更新 `status_bar.c`：電量顯示改為 "電量 --%"，字型改用 Noto Sans TC

## 3. Dashboard 頁面翻譯

- [x] 3.1 更新 `components/ui/screens/screen_dashboard.c`：休市狀態文字改為 "休市"、更新時間前綴改為 "更新: "
- [x] 3.2 更新 `screen_dashboard.c`：文字 label 字型從 Montserrat 改為 Noto Sans TC

## 4. Settings 頁面翻譯

- [x] 4.1 更新 `components/ui/screens/screen_settings.c`：標題改 "報價間隔"、選項改 "1 分鐘/5 分鐘/10 分鐘"、訊息改 "儲存成功/儲存失敗"
- [x] 4.2 更新 `screen_settings.c`：字型從 Montserrat 改為 Noto Sans TC

## 5. Info 頁面翻譯

- [x] 5.1 更新 `components/ui/screens/screen_info.c`：區段標題改為 "裝置/網路/AI/股票"
- [x] 5.2 更新 `screen_info.c`：狀態文字改為中文（已連線/連線中/離線/未儲存/未設定）
- [x] 5.3 更新 `screen_info.c`：資訊格式字串改為中文（記憶體/晶片/狀態/供應商/API Key）
- [x] 5.4 更新 `screen_info.c`：字型從 Montserrat 改為 Noto Sans TC

## 6. Portal 頁面翻譯

- [x] 6.1 更新 `components/ui/screens/screen_portal.c`：所有提示文字改為中文（入口已啟動/入口未啟動/加入 AP/密碼/網址）
- [x] 6.2 更新 `screen_portal.c`：操作說明改中文（1.加入 AP / 2.開啟瀏覽器 / 3.提交 WiFi）
- [x] 6.3 更新 `screen_portal.c`：字型從 Montserrat 改為 Noto Sans TC

## 7. Log 頁面翻譯

- [x] 7.1 更新 `components/ui/screens/screen_log.c`：標題改 "事件日誌"、分類標籤改 "股票/WiFi/AI/系統"
- [x] 7.2 更新 `screen_log.c`：字型從 Montserrat 改為 Noto Sans TC

## 8. 驗證

- [x] 8.1 執行 `./flash.sh --build-only` 確認完整編譯通過
