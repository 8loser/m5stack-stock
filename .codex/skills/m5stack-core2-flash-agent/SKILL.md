---
name: m5stack-core2-flash-agent
description: M5Stack Core2 燒錄與連線協作技能，專注 flash.sh 流程、序列埠監看、連線阻塞排除與實機 log 擷取。本技能不處理韌體功能邏輯修改。
---

# M5Stack Core2 Flash Agent

## Overview

聚焦實機連線、燒錄、monitor、log 擷取與燒錄流程阻塞排除。
需求若涉及 `components/` 或 `main/` 的功能邏輯修改，轉交 `m5stack-core2-dev`。

## Scope

- 可做：`flash.sh` build/flash/monitor/erase、port 佔用排查、log 擷取、連線路徑最小修補。
- 不做：UI/TWSE/scheduler/storage/network 等 firmware 功能邏輯調整。
- 允許修改：僅限 `flash.sh` 的連線與燒錄路徑；不得延伸到韌體功能。

## Container-First Policy

1. 燒錄與 monitor 預設走 `./flash.sh`，不直接在 host 上拼 `idf.py` 或 `esptool.py`。
2. `flash.sh` 會使用 container（`podman` 或 `docker`）執行 ESP-IDF 工具鏈。
3. 若流程卡住，先在 `flash.sh` 路徑內排錯；只有明確阻塞才考慮例外處理。

## Command Quick Reference

1. Build only：`./flash.sh --build-only`
2. Flash only：`./flash.sh --flash-only <port>`
3. App flash only：`./flash.sh --app-flash <port>`
4. Monitor：`./flash.sh --monitor <port>`
5. Erase + flash：`./flash.sh --erase --flash-only <port>`
6. Build + flash + monitor：依使用者要求串接，不自行擴大操作

## Standard Workflow

1. 先分類問題：build / port / flash / monitor。
2. 確認裝置 port（必要時列舉可用序列埠）。
3. 先走 `./flash.sh` 既有模式重現問題，不先繞開腳本。
4. 若需求包含建置，先做 `--build-only`。
5. 視需求執行 `--flash-only <port>` 或 `--app-flash <port>`。
6. 執行 `--monitor <port>` 擷取關鍵 log。
7. 若卡住，做最小修補與回歸驗證。
8. 產出結果摘要並交接（若需 firmware 修正）。

## Troubleshooting Decision Tree

1. `monitor` 啟動失敗
   - 先判斷是否 port 被佔用。
   - 清掉占用後重試同一命令。
2. 找不到裝置 port
   - 重新插拔裝置與線材後再探測。
   - 若仍失敗，回報目前可見埠清單與時間點。
3. `flash` 失敗
   - 先重試一次同命令，確認是否偶發連線抖動。
   - 再檢查 baud/port 參數是否與腳本一致。
4. 可 flash 但無法穩定 monitor
   - 優先收集最小可用 log 片段（啟動到錯誤）。
   - 必要時只做 log 擷取，不混入其他操作。

## Known Gotchas

1. `--monitor` 可能鎖住 port（例如 `/dev/ttyACM0`）
   - 燒錄前先釋放占用：`kill $(lsof -t /dev/ttyACM0)`
2. `build/` 若由 root 建立，container build 可能失敗
   - 先修正目錄擁有權，再重跑 `./flash.sh --build-only`

## Flash.sh Patch Policy

1. 只在連線/燒錄路徑阻塞時修改 `flash.sh`。
2. 允許修補範圍：serial lock 釋放/重試、序列埠偵測與權限相容、container runtime 可觀測性。
3. 變更要最小且可回滾，不做大規模重構。
4. 回報必須包含：阻塞原因、修補點、驗證命令。

## Handoff Format

交接 `m5stack-core2-dev` 時固定附上：

1. 使用命令（完整參數）
2. 裝置 port 與操作時間
3. 結果摘要（成功/失敗、重現率）
4. 關鍵錯誤行（最小必要片段）
5. 重現步驟（可直接再跑）
