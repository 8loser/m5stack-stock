#!/usr/bin/env bash
# ============================================================
# M5Stack Core2 Container 燒錄腳本
# 使用 espressif/idf 官方 Docker image，host 保持乾淨
#
# 用法:
#   ./flash.sh [PORT]              # build + flash + monitor
#   ./flash.sh --build-only        # 只 build
#   ./flash.sh --flash-only [PORT] # 只燒錄（需已 build）
#   ./flash.sh --app-flash [PORT]  # 只燒錄 app 分區（最快，日常開發建議）
#   ./flash.sh --monitor [PORT]    # 只開 monitor
#   ./flash.sh --erase [PORT]      # 清除整顆 flash 後再燒錄
#   ./flash.sh --shell             # 進入 container shell（除錯用）
#
# 需求：docker 或 podman（自動偵測）
# ============================================================

set -euo pipefail

IDF_IMAGE="docker.io/espressif/idf:v5.5.3"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BAUD=460800
MONITOR_BAUD=115200
CHIP=esp32

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

log()  { echo -e "${GREEN}[docker-flash]${NC} $*"; }
warn() { echo -e "${YELLOW}[docker-flash] WARN:${NC} $*"; }
err()  { echo -e "${RED}[docker-flash] ERR:${NC} $*"; exit 1; }

release_port_lock() {
    local port="$1"
    local pids=""

    [ -z "${port}" ] && return 0
    [ ! -e "${port}" ] && return 0

    pids="$(lsof -t "${port}" 2>/dev/null | sort -u | tr '\n' ' ' || true)"
    [ -z "${pids}" ] && return 0

    warn "${port} 目前被行程占用，嘗試自動釋放: ${pids}"
    # shellcheck disable=SC2086
    kill ${pids} 2>/dev/null || true
    sleep 1

    pids="$(lsof -t "${port}" 2>/dev/null | sort -u | tr '\n' ' ' || true)"
    if [ -n "${pids}" ]; then
        warn "${port} 仍被占用，改用強制終止: ${pids}"
        # shellcheck disable=SC2086
        kill -9 ${pids} 2>/dev/null || true
        sleep 1
    fi

    if lsof -t "${port}" >/dev/null 2>&1; then
        err "${port} 仍被占用，請手動釋放後重試"
    fi

    log "${port} 已釋放"
}

# ---------- 偵測 container runtime ----------
# 優先使用 podman（避免 Podman Docker CLI 模擬被誤判為真 Docker）
if command -v podman &>/dev/null; then
    RUNTIME=podman
elif command -v docker &>/dev/null; then
    RUNTIME=docker
else
    err "找不到 docker 或 podman！請先安裝：sudo pacman -S docker 或 sudo pacman -S podman"
fi
log "使用 container runtime: $RUNTIME"

# ---------- 解析參數 ----------
PORT=""
BUILD_ONLY=false
FLASH_ONLY=false
APP_FLASH_ONLY=false
MONITOR_ONLY=false
ERASE=false
SHELL_MODE=false

for arg in "$@"; do
    case "$arg" in
        --build-only)  BUILD_ONLY=true ;;
        --flash-only)  FLASH_ONLY=true ;;
        --app-flash)   APP_FLASH_ONLY=true ;;
        --monitor)     MONITOR_ONLY=true ;;
        --erase)       ERASE=true ;;
        --shell)       SHELL_MODE=true ;;
        /dev/*)        PORT="$arg" ;;
        COM*)          PORT="$arg" ;;
        *)             warn "未知參數: $arg" ;;
    esac
done

# 模式互斥檢查
mode_count=0
[ "$BUILD_ONLY" = true ]    && mode_count=$((mode_count + 1))
[ "$FLASH_ONLY" = true ]    && mode_count=$((mode_count + 1))
[ "$APP_FLASH_ONLY" = true ] && mode_count=$((mode_count + 1))
[ "$MONITOR_ONLY" = true ]  && mode_count=$((mode_count + 1))
[ "$SHELL_MODE" = true ]    && mode_count=$((mode_count + 1))
[ "$mode_count" -gt 1 ] && err "請只選擇一種模式：--build-only / --flash-only / --app-flash / --monitor / --shell"

# --erase 相容性檢查
if [ "$ERASE" = true ]; then
    if [ "$BUILD_ONLY" = true ] || [ "$MONITOR_ONLY" = true ] || [ "$SHELL_MODE" = true ]; then
        err "--erase 不相容於 --build-only / --monitor / --shell"
    fi
    if [ "$APP_FLASH_ONLY" = true ]; then
        warn "--app-flash 會忽略 --erase（app-only 模式不清空整顆 flash）"
    fi
fi

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
    release_port_lock "$PORT"

    if [ ! -w "$PORT" ]; then
        warn "$PORT 無寫入權限，嘗試修正..."
        sudo chmod 666 "$PORT" || err "無法存取 $PORT，請執行：sudo usermod -aG uucp \$USER 並重新登入"
    fi
fi

# ---------- 拉取 image（首次需要）----------
if ! $RUNTIME image inspect "$IDF_IMAGE" &>/dev/null; then
    log "首次使用，下載 IDF image（約 2GB，請耐心等待）..."
    $RUNTIME pull "$IDF_IMAGE"
fi

# ---------- 檢查 build/ 是否被 root 佔用 ----------
if [ -d "${PROJECT_DIR}/build" ] && [ "$(stat -c '%U' "${PROJECT_DIR}/build" 2>/dev/null)" = "root" ]; then
    warn "build/ 由 root 擁有（曾以 sudo 執行），正在清除..."
    sudo rm -rf "${PROJECT_DIR}/build" || err "請手動執行：sudo rm -rf build/"
    log "build/ 已清除"
fi

# ---------- 組合 container 執行參數 ----------
REAL_UID=$(id -u)
REAL_GID=$(id -g)

DOCKER_ARGS=(
    --rm
    --interactive
    --tty
    --volume "${PROJECT_DIR}:/project"
    --workdir /project
    -e HOME=/tmp                    # component manager 需要可寫的 HOME 目錄
    -e IDF_TARGET=esp32             # component manager 需要目標晶片資訊
)

# Podman rootless：host UID 映射到 container UID 0（root），
# --user 與此衝突導致 /project 無法寫入。
# 改用 --userns=keep-id：host UID 直接對應 container 同 UID，/project 可寫。
# Docker：用 --user 指定身份，build 產出物歸屬正確。
if [ "$RUNTIME" = "podman" ]; then
    DOCKER_ARGS+=(--userns=keep-id)
else
    DOCKER_ARGS+=(--user "${REAL_UID}:${REAL_GID}")
fi

# 需要 USB 時加入裝置
if [ -n "$PORT" ]; then
    DOCKER_ARGS+=(--device "${PORT}:${PORT}")
    # 讓 container 取得主機的 serial 群組權限（例如 Arch 的 uucp）
    if [ "$RUNTIME" = "podman" ]; then
        DOCKER_ARGS+=(--group-add keep-groups)
    else
        PORT_GID=$(stat -c '%g' "$PORT" 2>/dev/null || true)
        if [ -n "$PORT_GID" ]; then
            DOCKER_ARGS+=(--group-add "$PORT_GID")
        fi
    fi
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

elif [ "$APP_FLASH_ONLY" = true ]; then
    CMD="idf.py -p ${PORT} --baud ${BAUD} app-flash"

else
    # build + flash + monitor（預設）
    # set-target 只在首次（無 sdkconfig）時執行，避免每次都觸發 fullclean
    # update-dependencies 在 managed_components 不完整時也需執行
    if [ ! -f "${PROJECT_DIR}/sdkconfig" ]; then
        # update-dependencies 必須在 set-target 之前執行：
        # set-target 會觸發 CMake 配置，若 managed_components 不存在會找不到 LVGL 而失敗
        CMD="idf.py update-dependencies && idf.py set-target esp32 && idf.py build"
    elif [ ! -d "${PROJECT_DIR}/managed_components/lvgl__lvgl" ]; then
        CMD="idf.py update-dependencies && idf.py build"
    else
        CMD="idf.py build"
    fi

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
