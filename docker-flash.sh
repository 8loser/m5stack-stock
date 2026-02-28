#!/usr/bin/env bash
# ============================================================
# M5Stack Core2 Container 燒錄腳本
# 使用 espressif/idf 官方 Docker image，host 保持乾淨
#
# 用法:
#   ./docker-flash.sh              # build + flash + monitor
#   ./docker-flash.sh --build-only # 只 build
#   ./docker-flash.sh --flash-only # 只燒錄（需已 build）
#   ./docker-flash.sh --monitor    # 只開 monitor
#   ./docker-flash.sh --erase      # 清除 flash 後重新燒錄
#   ./docker-flash.sh --shell      # 進入 container shell（除錯用）
#
# 需求：docker 或 podman（自動偵測）
# ============================================================

set -euo pipefail

IDF_IMAGE="espressif/idf:v5.1.4"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BAUD=1500000
MONITOR_BAUD=115200
CHIP=esp32

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

log()  { echo -e "${GREEN}[docker-flash]${NC} $*"; }
warn() { echo -e "${YELLOW}[docker-flash] WARN:${NC} $*"; }
err()  { echo -e "${RED}[docker-flash] ERR:${NC} $*"; exit 1; }

# ---------- 偵測 container runtime ----------
if command -v docker &>/dev/null; then
    RUNTIME=docker
elif command -v podman &>/dev/null; then
    RUNTIME=podman
else
    err "找不到 docker 或 podman！請先安裝：sudo pacman -S docker 或 sudo pacman -S podman"
fi
log "使用 container runtime: $RUNTIME"

# ---------- 解析參數 ----------
PORT=""
BUILD_ONLY=false
FLASH_ONLY=false
MONITOR_ONLY=false
ERASE=false
SHELL_MODE=false

for arg in "$@"; do
    case "$arg" in
        --build-only)  BUILD_ONLY=true ;;
        --flash-only)  FLASH_ONLY=true ;;
        --monitor)     MONITOR_ONLY=true ;;
        --erase)       ERASE=true ;;
        --shell)       SHELL_MODE=true ;;
        /dev/*)        PORT="$arg" ;;
        COM*)          PORT="$arg" ;;
        *)             warn "未知參數: $arg" ;;
    esac
done

# ---------- 自動偵測 Port ----------
if [ -z "$PORT" ] && [ "$BUILD_ONLY" = false ] && [ "$SHELL_MODE" = false ]; then
    for candidate in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        if [ -e "$candidate" ]; then
            PORT="$candidate"
            log "找到 port: $PORT"
            break
        fi
    done
    if [ -z "$PORT" ]; then
        err "找不到 USB port！請接上 M5Stack Core2（CP2104 驅動）"
    fi
fi

# ---------- 確認 port 權限 ----------
if [ -n "$PORT" ] && [ -e "$PORT" ]; then
    if [ ! -w "$PORT" ]; then
        warn "$PORT 無寫入權限，嘗試修正..."
        sudo chmod 666 "$PORT" || err "無法存取 $PORT，請執行：sudo usermod -a -G dialout \$USER 並重新登入"
    fi
fi

# ---------- 拉取 image（首次需要）----------
if ! $RUNTIME image inspect "$IDF_IMAGE" &>/dev/null; then
    log "首次使用，下載 IDF image（約 2GB，請耐心等待）..."
    $RUNTIME pull "$IDF_IMAGE"
fi

# ---------- 組合 container 執行參數 ----------
DOCKER_ARGS=(
    --rm
    --interactive
    --tty
    --volume "${PROJECT_DIR}:/project"
    --workdir /project
    --user "$(id -u):$(id -g)"      # 使用目前用戶身份，build 產出物歸屬正確
)

# 需要 USB 時加入裝置
if [ -n "$PORT" ]; then
    DOCKER_ARGS+=(--device "${PORT}:${PORT}")
fi

# Podman 需要額外權限存取 USB
if [ "$RUNTIME" = "podman" ]; then
    DOCKER_ARGS+=(--userns=keep-id)
fi

# ---------- Shell 模式 ----------
if [ "$SHELL_MODE" = true ]; then
    log "進入 IDF container shell（輸入 exit 離開）..."
    $RUNTIME run "${DOCKER_ARGS[@]}" "$IDF_IMAGE" bash
    exit 0
fi

# ---------- 組合要執行的指令 ----------
CMD=""

if [ "$MONITOR_ONLY" = true ]; then
    CMD="idf.py -p ${PORT} --baud ${MONITOR_BAUD} monitor"

elif [ "$FLASH_ONLY" = true ]; then
    if [ "$ERASE" = true ]; then
        CMD="esptool.py --chip ${CHIP} --port ${PORT} --baud ${BAUD} erase_flash && "
    fi
    CMD+="idf.py -p ${PORT} --baud ${BAUD} flash"

else
    # build + flash + monitor（預設）
    CMD="idf.py build"

    if [ "$BUILD_ONLY" = false ]; then
        if [ "$ERASE" = true ]; then
            CMD+=" && esptool.py --chip ${CHIP} --port ${PORT} --baud ${BAUD} erase_flash"
        fi
        CMD+=" && idf.py -p ${PORT} --baud ${BAUD} flash"
        CMD+=" && idf.py -p ${PORT} --baud ${MONITOR_BAUD} monitor"
    fi
fi

# ---------- 執行 ----------
log "執行：$CMD"
echo -e "${CYAN}============================================${NC}"

$RUNTIME run "${DOCKER_ARGS[@]}" "$IDF_IMAGE" bash -c "$CMD"

echo -e "${CYAN}============================================${NC}"
log "完成"
