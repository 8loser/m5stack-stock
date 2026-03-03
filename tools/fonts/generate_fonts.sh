#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FONT_DIR="${PROJECT_ROOT}/components/ui/fonts"

OUT_14="${FONT_DIR}/lv_font_noto_tc_14.c"
OUT_16="${FONT_DIR}/lv_font_noto_tc_16.c"
FONT_PATH_DEFAULT="${SCRIPT_DIR}/NotoSansTC-Regular.ttf"
TWSE_URL="https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL"
TWSE_INDUSTRY_URL="https://openapi.twse.com.tw/v1/company/getStockInfo"
TWSE_INDUSTRY_URL_FALLBACK="https://openapi.twse.com.tw/v1/opendata/t187ap03_L"

FETCH_SCRIPT="${SCRIPT_DIR}/fetch_stock_chars.py"
EXTRACT_SCRIPT="${SCRIPT_DIR}/extract_ui_symbols.py"
UI_SYMBOLS_FILE="${SCRIPT_DIR}/ui_symbols.txt"
TWSE_SYMBOLS_FILE="${SCRIPT_DIR}/twse_symbols.txt"
INDUSTRY_SYMBOLS_FILE="${SCRIPT_DIR}/industry_symbols.txt"
TMP_JSON="$(mktemp)"
TMP_INDUSTRY_JSON="$(mktemp)"
trap 'rm -f "${TMP_JSON}" "${TMP_INDUSTRY_JSON}"' EXIT

print_usage() {
    cat <<'EOF'
Usage:
  tools/fonts/generate_fonts.sh [--offline|--online] [--font <path>] [font_path]

Modes:
  --offline  Use existing twse_symbols.txt without network (default)
  --online   Download TWSE JSON and refresh stock-name + industry symbols before generating

Options:
  --font <path>  Path to NotoSansTC-Regular.ttf
EOF
}

MODE="offline"
FONT_PATH="${FONT_PATH_DEFAULT}"
POSITIONAL_FONT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --offline)
        MODE="offline"
        shift
        ;;
    --online)
        MODE="online"
        shift
        ;;
    --font)
        if [[ $# -lt 2 ]]; then
            echo "Error: --font requires a path argument" >&2
            print_usage >&2
            exit 1
        fi
        FONT_PATH="$2"
        shift 2
        ;;
    --help|-h)
        print_usage
        exit 0
        ;;
    --*)
        echo "Error: unknown option: $1" >&2
        print_usage >&2
        exit 1
        ;;
    *)
        if [[ -n "${POSITIONAL_FONT}" ]]; then
            echo "Error: multiple font paths provided" >&2
            print_usage >&2
            exit 1
        fi
        POSITIONAL_FONT="$1"
        shift
        ;;
    esac
done

if [[ -n "${POSITIONAL_FONT}" ]]; then
    FONT_PATH="${POSITIONAL_FONT}"
fi

if [[ ! -f "${EXTRACT_SCRIPT}" ]]; then
    echo "Error: missing UI symbol extractor: ${EXTRACT_SCRIPT}" >&2
    exit 1
fi

if [[ ! -f "${FETCH_SCRIPT}" ]]; then
    echo "Error: missing parser script: ${FETCH_SCRIPT}" >&2
    exit 1
fi

if [[ ! -f "${FONT_PATH}" ]]; then
    echo "Font not found: ${FONT_PATH}" >&2
    print_usage >&2
    exit 1
fi

if ! python3 "${EXTRACT_SCRIPT}" \
    --src "${PROJECT_ROOT}/components/ui" \
    --src "${PROJECT_ROOT}/components/twse_client" \
    --out "${UI_SYMBOLS_FILE}"; then
    echo "Error: failed to update UI symbols" >&2
    exit 1
fi

if [[ ! -s "${UI_SYMBOLS_FILE}" ]]; then
    echo "Error: UI symbols file is empty: ${UI_SYMBOLS_FILE}" >&2
    exit 1
fi

if [[ "${MODE}" == "online" ]]; then
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

    if ! python3 "${FETCH_SCRIPT}" --input-json "${TMP_JSON}" > "${TWSE_SYMBOLS_FILE}"; then
        echo "Error: failed to generate TWSE symbols" >&2
        exit 1
    fi

    if [[ ! -s "${TWSE_SYMBOLS_FILE}" ]]; then
        echo "Error: generated TWSE symbols are empty: ${TWSE_SYMBOLS_FILE}" >&2
        exit 1
    fi

    rm -f "${INDUSTRY_SYMBOLS_FILE}"
    industry_url_used=""
    for industry_url in "${TWSE_INDUSTRY_URL}" "${TWSE_INDUSTRY_URL_FALLBACK}"; do
        if ! curl "${CURL_OPTS[@]}" "${industry_url}" -o "${TMP_INDUSTRY_JSON}" 2>/dev/null; then
            continue
        fi

        python3 "${FETCH_SCRIPT}" --mode industry --input-json "${TMP_INDUSTRY_JSON}" > "${INDUSTRY_SYMBOLS_FILE}" || true
        if [[ -s "${INDUSTRY_SYMBOLS_FILE}" ]]; then
            industry_url_used="${industry_url}"
            break
        fi
        rm -f "${INDUSTRY_SYMBOLS_FILE}"
    done

    if [[ -z "${industry_url_used}" ]]; then
        echo "Warning: failed to download/parse industry JSON, skip dynamic industry symbols" >&2
    fi
else
    if [[ ! -s "${TWSE_SYMBOLS_FILE}" ]]; then
        echo "Error: offline mode requires non-empty ${TWSE_SYMBOLS_FILE}" >&2
        exit 1
    fi
fi

FILES_TO_MERGE=("${UI_SYMBOLS_FILE}" "${TWSE_SYMBOLS_FILE}")
if [[ -s "${INDUSTRY_SYMBOLS_FILE}" ]]; then
    FILES_TO_MERGE+=("${INDUSTRY_SYMBOLS_FILE}")
fi

SYMBOLS="$(python3 - "${FILES_TO_MERGE[@]}" <<'PY'
import sys

chars = set()
for path in sys.argv[1:]:
    with open(path, 'r', encoding='utf-8') as f:
        chars.update(ch for ch in f.read() if not ch.isspace())

print(''.join(sorted(chars)), end='')
PY
)"

if [[ -z "${SYMBOLS}" ]]; then
    echo "Error: merged symbols are empty" >&2
    exit 1
fi

echo "使用擴充字符集（UI + TWSE 股票名稱 + 產業別），模式: ${MODE}"

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
echo "  ${UI_SYMBOLS_FILE}"
echo "  ${TWSE_SYMBOLS_FILE}"
if [[ -s "${INDUSTRY_SYMBOLS_FILE}" ]]; then
    echo "  ${INDUSTRY_SYMBOLS_FILE}"
fi
