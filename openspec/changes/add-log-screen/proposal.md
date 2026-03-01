## Why

使用者在裝置上無法查看系統活動歷史。目前發生的股票更新、WiFi 狀態變化、AI 分析結果、系統事件只能從 monitor log 觀察，裝置螢幕上沒有任何顯示。

## What Changes

- 新增 `SCREEN_LOG = 2` 頁面，以環形緩衝區（32 條）記錄四類應用層事件
- 支援四種 tag：STOCK / WIFI / AI / SYS，三種 level：INFO / WARN / ERROR，各有對應顏色
- `screen_log_push()` thread-safe（portMUX 保護），可從任何 FreeRTOS task 呼叫
- 頁面顯示最多 12 行，最新條目在最上方（倒序）
- 導航：Dashboard 左鍵（btn=0）進入 Log，Log 左鍵返回 Dashboard，Log 右鍵進 Portal
- main.c 補齊 AI result queue 消費，并以批次摘要方式寫入 Stock log 避免塞爆 ring buffer

## Capabilities

### New Capabilities

- `log-page`: 系統事件歷史頁面，顯示 STOCK / WIFI / AI / SYS 四類彩色事件條目

### Modified Capabilities

- `ui-manager`: 新增 `SCREEN_LOG` enum、頁面初始化、switch_screen 刷新、hw_button 路由、四個 thread-safe log API
- `status-bar`: `refresh_page_message()` 新增 `SCREEN_LOG → "Event Log"` 分支
- `main`: WiFi callback 補 log、啟動補 SYS log、Quote 消費改批次摘要、補 AI result queue 消費

## Impact

- `components/ui/include/screen_log.h` — 新增：log_tag_t / log_level_t enum、push/create/refresh 宣告
- `components/ui/screens/screen_log.c` — 新增：ring buffer、12 個靜態 label、顏色對應邏輯
- `components/ui/include/ui_manager.h` — 修改：新增 `SCREEN_LOG` enum、四個 log API 宣告
- `components/ui/ui_manager.c` — 修改：s_screens 擴為 3、init 加 screen_log_create、switch_screen 加刷新、handle_hw_button 更新路由、實作四個 log API
- `components/ui/widgets/status_bar.c` — 修改：refresh_page_message 加 SCREEN_LOG 分支
- `components/ui/CMakeLists.txt` — 修改：SRCS 加 screen_log.c
- `main/main.c` — 修改：補 WiFi/SYS log、Quote 批次摘要、AI result queue 消費
