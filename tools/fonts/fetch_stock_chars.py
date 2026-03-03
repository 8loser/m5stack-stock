#!/usr/bin/env python3
import argparse
import json
import sys


TWSE_NAME_KEYS = (
    "證券名稱",
    "公司名稱",
    "公司簡稱",
    "股票名稱",
    "Name",
    "CompanyName",
    "SecurityName",
)

TWSE_INDUSTRY_KEYS = (
    "產業別",
    "業別",
    "industry",
    "Industry",
)

# Keep in sync with components/twse_client/twse_client.c s_industry_map.
INDUSTRY_CODE_MAP = {
    "1": "水泥工業",
    "2": "食品工業",
    "3": "塑膠工業",
    "4": "紡織纖維",
    "5": "電機機械",
    "6": "電器電纜",
    "8": "玻璃陶瓷",
    "9": "造紙工業",
    "10": "鋼鐵工業",
    "11": "橡膠工業",
    "12": "汽車工業",
    "14": "建材營造",
    "15": "航運業",
    "16": "觀光餐旅",
    "17": "金融保險",
    "18": "貿易百貨",
    "19": "綜合",
    "20": "其他",
    "21": "化學工業",
    "22": "生技醫療",
    "23": "油電燃氣",
    "24": "半導體業",
    "25": "電腦及週邊設備業",
    "26": "光電業",
    "27": "通信網路業",
    "28": "電子零組件業",
    "29": "電子通路業",
    "30": "資訊服務業",
    "31": "其他電子業",
    "32": "文化創意業",
    "33": "農業科技業",
    "34": "電子商務",
    "35": "綠能環保",
    "36": "數位雲端",
    "37": "運動休閒",
    "38": "居家生活",
}


def to_records(data):
    if isinstance(data, dict):
        if isinstance(data.get("data"), list):
            return data["data"]
        return [data]
    if isinstance(data, list):
        return data
    raise ValueError("unexpected JSON structure: expected list or object")


def load_json(input_path: str):
    if input_path == "-":
        text = sys.stdin.read().strip()
    else:
        with open(input_path, "r", encoding="utf-8-sig", errors="replace") as f:
            text = f.read().strip()

    if not text:
        raise ValueError("empty JSON input")

    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        snippet = text[:120].replace("\n", " ")
        raise ValueError(f"non-JSON input: {snippet!r}") from exc


def extract_twse_names(data):
    records = to_records(data)
    names = []
    for item in records:
        if not isinstance(item, dict):
            continue
        name = None
        for key in TWSE_NAME_KEYS:
            if item.get(key):
                name = item.get(key)
                break
        if name:
            names.append(name)
    if not names:
        raise ValueError("no stock names found in TWSE response")
    return names


def normalize_industry_code(value: str) -> str:
    code = value.strip()
    if not code.isdigit():
        return code
    code = code.lstrip("0")
    return code if code else "0"


def extract_twse_industries(data) -> list[str]:
    records = to_records(data)
    industries = []

    for item in records:
        if not isinstance(item, dict):
            continue

        raw_value = None
        for key in TWSE_INDUSTRY_KEYS:
            value = item.get(key)
            if value is None:
                continue
            if isinstance(value, str):
                value = value.strip()
                if not value or value == "-":
                    continue
            raw_value = value
            break

        if raw_value is None:
            continue

        if isinstance(raw_value, (int, float)):
            normalized = normalize_industry_code(str(int(raw_value)))
            mapped = INDUSTRY_CODE_MAP.get(normalized)
            if mapped:
                industries.append(mapped)
            continue

        if isinstance(raw_value, str):
            normalized = normalize_industry_code(raw_value)
            mapped = INDUSTRY_CODE_MAP.get(normalized)
            if mapped:
                industries.append(mapped)
            elif not normalized.isdigit():
                industries.append(normalized)

    if industries:
        return industries

    return list(INDUSTRY_CODE_MAP.values())


def is_cjk_unified(ch: str) -> bool:
    codepoint = ord(ch)
    return 0x4E00 <= codepoint <= 0x9FFF


def collect_unique_cjk(texts):
    chars = set()
    for text in texts:
        for ch in text:
            if is_cjk_unified(ch):
                chars.add(ch)
    return chars


def parse_args():
    parser = argparse.ArgumentParser(
        description="Extract TWSE stock-name or industry CJK chars from JSON"
    )
    parser.add_argument(
        "--input-json",
        default="-",
        help="Input JSON path, use '-' to read stdin (default: -)",
    )
    parser.add_argument(
        "--mode",
        choices=["name", "industry"],
        default="name",
        help="Extraction mode: stock name or industry (default: name)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        data = load_json(args.input_json)
        if args.mode == "industry":
            all_names = extract_twse_industries(data)
        else:
            all_names = extract_twse_names(data)
    except Exception as exc:
        print(f"Error: failed to parse TWSE data ({args.mode}): {exc}", file=sys.stderr)
        sys.exit(1)

    cjk_chars = collect_unique_cjk(all_names)
    output = "".join(sorted(cjk_chars))
    if not output:
        print("Error: no CJK symbols extracted from stock names", file=sys.stderr)
        sys.exit(1)

    sys.stdout.write(output)


if __name__ == "__main__":
    main()
