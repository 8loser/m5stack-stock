#!/usr/bin/env bash
# ============================================================
# M5Stack Core2 快速燒錄腳本
# 用法:
#   ./flash.sh              # 自動偵測 port，build + flash + monitor
#   ./flash.sh /dev/ttyUSB0 # 指定 port
#   ./flash.sh --build-only # 只 build，不燒錄
#   ./flash.sh --flash-only # 只燒錄（不 build，不 monitor）
#   ./flash.sh --monitor    # 只開 monitor
#   ./flash.sh --erase      # 清除 flash 後重新燒錄
# ============================================================

set -euo pipefail

# ---------- 設定區 ----------
BAUD=1500000          # 燒錄速度（Core2 最高穩定 1.5M）
MONITOR_BAUD=115200   # Monitor 速度
CHIP=esp32
FLASH_SIZE=16MB

# ---------- 顏色輸出 ----------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

log()  { echo -e "${GREEN}[flash.sh]${NC} $*"; }
warn() { echo -e "${YELLOW}[flash.sh] WARN:${NC} $*"; }
err()  { echo -e "${RED}[flash.sh] ERR:${NC} $*"; exit 1; }

# ---------- 解析參數 ----------
PORT=""
BUILD_ONLY=false
FLASH_ONLY=false
MONITOR_ONLY=false
ERASE=false

for arg in "$@"; do
    case "$arg" in
        --build-only)  BUILD_ONLY=true ;;
        --flash-only)  FLASH_ONLY=true ;;
        --monitor)     MONITOR_ONLY=true ;;
        --erase)       ERASE=true ;;
        /dev/*)        PORT="$arg" ;;
        COM*)          PORT="$arg" ;;
        *)             warn "未知參數: $arg" ;;
    esac
done

# ---------- 確認 IDF 環境 ----------
if [ -z "${IDF_PATH:-}" ]; then
    # 嘗試自動 source
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        log "自動載入 IDF 環境: $HOME/esp/esp-idf/export.sh"
        source "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1
    elif [ -f "/opt/esp-idf/export.sh" ]; then
        source "/opt/esp-idf/export.sh" > /dev/null 2>&1
    else
        err "找不到 IDF 環境！請先執行: source \$IDF_PATH/export.sh"
    fi
fi
log "IDF 版本: $(idf.py --version 2>/dev/null || echo '未知')"

# ---------- 自動偵測 Port ----------
if [ -z "$PORT" ] && [ "$BUILD_ONLY" = false ]; then
    log "自動偵測 M5Stack Core2 port..."
    # 優先找 CP2104 (M5Stack Core2 USB-Serial)
    for candidate in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial*; do
        if [ -e "$candidate" ]; then
            PORT="$candidate"
            log "找到 port: $PORT"
            break
        fi
    done
    if [ -z "$PORT" ]; then
        err "找不到 USB port！請接上 M5Stack Core2 並確認驅動已安裝（CP2104）"
    fi
fi

# 確認 port 可存取
if [ -n "$PORT" ] && [ ! -w "$PORT" ]; then
    warn "Port $PORT 無寫入權限，嘗試加入 dialout 群組..."
    warn "請執行: sudo usermod -a -G dialout \$USER  然後重新登入"
    warn "或: sudo chmod 666 $PORT"
fi

# ---------- Monitor only ----------
if [ "$MONITOR_ONLY" = true ]; then
    log "開啟 Monitor (port=$PORT baud=$MONITOR_BAUD)..."
    idf.py -p "$PORT" --baud "$MONITOR_BAUD" monitor
    exit 0
fi

# ---------- Build ----------
if [ "$FLASH_ONLY" = false ]; then
    log "開始 Build..."
    echo -e "${CYAN}========================================${NC}"
    idf.py build
    echo -e "${CYAN}========================================${NC}"
    log "Build 完成！"

    # 顯示 binary 大小
    if [ -f "build/m5stack_stock.bin" ]; then
        SIZE=$(du -h build/m5stack_stock.bin | cut -f1)
        log "Binary 大小: $SIZE"
    fi
fi

if [ "$BUILD_ONLY" = true ]; then
    log "僅 Build 完成，跳過燒錄"
    exit 0
fi

# ---------- Erase（選用）----------
if [ "$ERASE" = true ]; then
    warn "清除 Flash（$PORT）..."
    esptool.py --chip "$CHIP" --port "$PORT" --baud "$BAUD" erase_flash
    log "Flash 清除完成"
fi

# ---------- Flash ----------
log "燒錄中... (port=$PORT baud=$BAUD)"
echo -e "${CYAN}========================================${NC}"
idf.py -p "$PORT" --baud "$BAUD" flash
echo -e "${CYAN}========================================${NC}"
log "燒錄完成！"

# ---------- Monitor ----------
if [ "$FLASH_ONLY" = false ]; then
    log "開啟 Monitor (Ctrl+] 退出)..."
    idf.py -p "$PORT" --baud "$MONITOR_BAUD" monitor
fi
