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
    if isinstance(data, dict):
        if isinstance(data.get("data"), list):
            records = data["data"]
        else:
            records = [data]
    elif isinstance(data, list):
        records = data
    else:
        raise ValueError("unexpected JSON structure: expected list or object")

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
    parser = argparse.ArgumentParser(description="Extract TWSE stock-name CJK chars from JSON")
    parser.add_argument(
        "--input-json",
        default="-",
        help="Input JSON path, use '-' to read stdin (default: -)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        data = load_json(args.input_json)
        all_names = extract_twse_names(data)
    except Exception as exc:
        print(f"Error: failed to parse stock names: {exc}", file=sys.stderr)
        sys.exit(1)

    cjk_chars = collect_unique_cjk(all_names)
    output = "".join(sorted(cjk_chars))
    if not output:
        print("Error: no CJK symbols extracted from stock names", file=sys.stderr)
        sys.exit(1)

    sys.stdout.write(output)


if __name__ == "__main__":
    main()
