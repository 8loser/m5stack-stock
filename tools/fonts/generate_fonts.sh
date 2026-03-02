#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FONT_DIR="${PROJECT_ROOT}/components/ui/fonts"

OUT_14="${FONT_DIR}/lv_font_noto_tc_14.c"
OUT_16="${FONT_DIR}/lv_font_noto_tc_16.c"
FONT_PATH_DEFAULT="${SCRIPT_DIR}/NotoSansTC-Regular.ttf"
FONT_PATH="${1:-${FONT_PATH_DEFAULT}}"
TWSE_URL="https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL"

FETCH_SCRIPT="${SCRIPT_DIR}/fetch_stock_chars.py"
SYMBOLS_FILE="${SCRIPT_DIR}/twse_symbols.txt"
TMP_JSON="$(mktemp)"
trap 'rm -f "${TMP_JSON}"' EXIT

if [[ ! -f "${FETCH_SCRIPT}" ]]; then
    echo "Error: missing parser script: ${FETCH_SCRIPT}" >&2
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "Error: curl is required but not found" >&2
    exit 1
fi

CURL_OPTS=(--fail --silent --show-error --location --max-time 20)
downloaded=false
used_insecure_curl=false

if curl "${CURL_OPTS[@]}" "${TWSE_URL}" -o "${TMP_JSON}" 2>/dev/null; then
    downloaded=true
elif curl "${CURL_OPTS[@]}" -k "${TWSE_URL}" -o "${TMP_JSON}" 2>/dev/null; then
    downloaded=true
    used_insecure_curl=true
fi

if [[ "${downloaded}" != true ]]; then
    echo "Error: failed to download TWSE JSON from ${TWSE_URL}" >&2
    exit 1
fi

if [[ "${used_insecure_curl}" == true ]]; then
    echo "Warning: 憑證驗證失敗，改用 curl -k 下載 TWSE JSON" >&2
fi

if ! python3 "${FETCH_SCRIPT}" --input-json "${TMP_JSON}" > "${SYMBOLS_FILE}"; then
    echo "Error: failed to generate symbols file" >&2
    exit 1
fi

if [[ ! -s "${SYMBOLS_FILE}" ]]; then
    echo "Error: generated symbols file is empty: ${SYMBOLS_FILE}" >&2
    exit 1
fi

if [[ ! -f "${FONT_PATH}" ]]; then
    echo "Font not found: ${FONT_PATH}"
    echo "Please provide NotoSansTC-Regular.ttf path:"
    echo "  ${BASH_SOURCE[0]} /path/to/NotoSansTC-Regular.ttf"
    exit 1
fi

SYMBOLS="$(cat "${SYMBOLS_FILE}")"
echo "使用擴充字符集（UI + TWSE 股票名稱）"

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
echo "  ${SYMBOLS_FILE}"
