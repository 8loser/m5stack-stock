#!/usr/bin/env python3
import argparse
import ast
import os
import re
import sys
from pathlib import Path

STRING_RE = re.compile(r'(?P<prefix>u8|u|U|L)?"(?P<body>(?:\\.|[^"\\])*)"', re.DOTALL)
COMMENT_OR_WS_RE = re.compile(r'(?:\s+|//[^\n]*\n|/\*.*?\*/)+', re.DOTALL)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract non-ASCII UI symbols from C string literals"
    )
    script_dir = Path(__file__).resolve().parent
    parser.add_argument(
        "--src",
        action="append",
        dest="srcs",
        default=None,
        help="Source directory to scan, repeatable (default: components/ui)",
    )
    parser.add_argument(
        "--out",
        default=str(script_dir / "ui_symbols.txt"),
        help="Output file path (default: tools/fonts/ui_symbols.txt)",
    )
    return parser.parse_args()


def decode_c_string(literal_body: str) -> str:
    return ast.literal_eval(f'"{literal_body}"')


def skip_gap(text: str, pos: int) -> int:
    while True:
        m = COMMENT_OR_WS_RE.match(text, pos)
        if not m:
            return pos
        pos = m.end()


def extract_strings(text: str) -> list[str]:
    out: list[str] = []
    pos = 0
    length = len(text)

    while pos < length:
        m = STRING_RE.search(text, pos)
        if not m:
            break

        parts = [decode_c_string(m.group("body"))]
        pos = m.end()

        while True:
            pos = skip_gap(text, pos)
            m2 = STRING_RE.match(text, pos)
            if not m2:
                break
            parts.append(decode_c_string(m2.group("body")))
            pos = m2.end()

        out.append("".join(parts))

    return out


def iter_source_files(src_dir: Path):
    for path in src_dir.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in {".c", ".h"}:
            continue
        if "fonts" in path.parts:
            continue
        yield path


def collect_symbols(src_dirs: list[Path]) -> str:
    chars: set[str] = set()
    found_file = False

    for src_dir in src_dirs:
        for path in iter_source_files(src_dir):
            found_file = True
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError as exc:
                raise RuntimeError(f"failed to read {path}: {exc}") from exc

            for s in extract_strings(text):
                for ch in s:
                    if ord(ch) > 0x7F:
                        chars.add(ch)

    if not found_file:
        raise RuntimeError("no .c/.h source files found")

    if not chars:
        raise RuntimeError("no non-ASCII symbols extracted from UI sources")

    return "".join(sorted(chars))


def main() -> int:
    args = parse_args()
    if args.srcs:
        src_dirs = [Path(src).resolve() for src in args.srcs]
    else:
        src_dirs = [Path(__file__).resolve().parent.parent.parent / "components" / "ui"]
    out_path = Path(args.out).resolve()

    for src_dir in src_dirs:
        if not src_dir.exists() or not src_dir.is_dir():
            print(f"Error: source directory not found: {src_dir}", file=sys.stderr)
            return 1
        if not os.access(src_dir, os.R_OK | os.X_OK):
            print(f"Error: source directory not readable: {src_dir}", file=sys.stderr)
            return 1

    try:
        symbols = collect_symbols(src_dirs)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(symbols + "\n", encoding="utf-8")
    except OSError as exc:
        print(f"Error: failed to write {out_path}: {exc}", file=sys.stderr)
        return 1

    print(f"Wrote {len(symbols)} symbols to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
