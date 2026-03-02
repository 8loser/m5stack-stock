#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_14="${SCRIPT_DIR}/lv_font_noto_tc_14.c"
OUT_16="${SCRIPT_DIR}/lv_font_noto_tc_16.c"

FONT_PATH_DEFAULT="${SCRIPT_DIR}/NotoSansTC-Regular.ttf"
FONT_PATH="${1:-${FONT_PATH_DEFAULT}}"

SYMBOLS="儀表板日誌資訊設定入口已連線中離線電量更新休市報價間隔分鐘儲存成功失敗裝置網路AI股票記憶體晶片狀態供應商金鑰密碼網址事件系統掃描加入啟動等待自動再開啟瀏覽器提交未秒僅盤是否代號硬體測試震動音效嗶聲"

if [[ ! -f "${FONT_PATH}" ]]; then
    echo "Font not found: ${FONT_PATH}"
    echo "Please provide NotoSansTC-Regular.ttf path:"
    echo "  ${BASH_SOURCE[0]} /path/to/NotoSansTC-Regular.ttf"
    exit 1
fi

run_conv() {
    local size="$1"
    local output="$2"
    lv_font_conv \
        --no-compress \
        --no-prefilter \
        --bpp 4 \
        --size "${size}" \
        --font "${FONT_PATH}" \
        -r 0x20-0x7F \
        --symbols "${SYMBOLS}" \
        --format lvgl \
        --lv-include lvgl.h \
        -o "${output}" \
        --force-fast-kern-format
}

run_conv 14 "${OUT_14}"
run_conv 16 "${OUT_16}"

echo "Generated:"
echo "  ${OUT_14}"
echo "  ${OUT_16}"
