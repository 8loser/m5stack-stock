## Why

目前使用者缺乏一個可以快速判斷「系統是否當機」的可視化訊號。現況即使主流程（`main`/`scheduler`）異常，UI 仍可能持續刷新，造成誤判為系統正常。

## What Changes

- 在共用 status bar 新增主流程心跳指標，位置為時間前方。
- 心跳採雙擊節奏動畫，正常時顯示愛心 emoji；主流程逾時時改為 `!` 並停止閃爍。
- 新增 `ui_manager` 心跳 API，分別由 `main` 與 `scheduler` 餵入活性時間戳。
- `scheduler_task` 迴圈等待粒度由 60 秒改為 1 秒，以提供可判斷的活性更新頻率。
- 擴充字型字符集來源以包含心跳所需 emoji，並規範渲染失敗時 fallback 到 ASCII `<3`。

## Capabilities

### New Capabilities
- `main-flow-liveness-indicator`: 以 status bar 心跳指標顯示主流程是否仍存活

### Modified Capabilities
- `ui-manager`: 新增主流程心跳資料模型、公開 API 與 status bar 心跳呈現規則
- `scheduler`: 新增排程任務活性餵心跳與 1 秒等待粒度要求
- `main`: 新增主迴圈活性餵心跳要求
- `cjk-font-system`: 新增心跳 emoji 字型輸出與 fallback 規範

## Impact

- `components/ui/include/ui_manager.h`
- `components/ui/ui_manager.c`
- `components/ui/widgets/status_bar.c`
- `components/scheduler/scheduler.c`
- `main/main.c`
- `tools/fonts/ui_symbols.txt`
- `tools/fonts/fetch_stock_chars.py`（必要時）
- `tools/fonts/generate_fonts.sh`（必要時）
