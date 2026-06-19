#!/usr/bin/env python3
"""Update docs/libretro-built-cores.tsv from a libretro build manifest."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Refresh the build-related columns in docs/libretro-built-cores.tsv "
            "from dist/*/docs/manifest.txt."
        )
    )
    parser.add_argument(
        "--manifest",
        default=ROOT / "dist/plumos-libretro-cores-onion-lock-all/docs/manifest.txt",
        type=Path,
    )
    parser.add_argument(
        "--input",
        default=ROOT / "docs/libretro-built-cores.tsv",
        type=Path,
    )
    parser.add_argument(
        "--output",
        default=None,
        type=Path,
        help="Output path. Defaults to stdout.",
    )
    return parser.parse_args()


def parse_manifest(path: Path) -> dict[str, dict[str, str]]:
    sections: dict[str, dict[str, str]] = {}
    current = ""
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            sections.setdefault(current, {})
            continue
        if not current or "=" not in line:
            continue
        key, value = line.split("=", 1)
        sections[current][key.strip()] = value.strip()
    return {
        core_id: data
        for core_id, data in sections.items()
        if data.get("status") == "built" and data.get("commit")
    }


def read_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        return list(reader.fieldnames or []), list(reader)


def write_rows(path: Path | None, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    import sys

    handle = path.open("w", encoding="utf-8", newline="") if path else sys.stdout
    close = path is not None
    try:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
            delimiter="\t",
            lineterminator="\n",
            extrasaction="ignore",
        )
        writer.writeheader()
        writer.writerows(rows)
    finally:
        if close:
            handle.close()


def main() -> int:
    args = parse_args()
    built = parse_manifest(args.manifest)
    fieldnames, rows = read_rows(args.input)
    if not fieldnames:
        raise SystemExit(f"empty TSV: {args.input}")

    seen: set[str] = set()
    for row in rows:
        core_id = row.get("core_id", "")
        data = built.get(core_id)
        if not data:
            continue
        seen.add(core_id)
        row["class"] = data.get("class", row.get("class", ""))
        row["make_args"] = data.get("make_args", row.get("make_args", ""))
        row["make_jobs"] = data.get("make_jobs", row.get("make_jobs", ""))
        row["serial_jobs"] = "yes" if data.get("make_jobs") == "1" else "no"
        row["commit"] = data.get("commit", row.get("commit", ""))
        row["log"] = data.get("log", row.get("log", ""))

    missing = sorted(set(built) - seen)
    if missing:
        raise SystemExit(
            "build manifest contains cores missing from inventory: " + ", ".join(missing)
        )

    write_rows(args.output, fieldnames, rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
