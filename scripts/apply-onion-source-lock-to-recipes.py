#!/usr/bin/env python3
"""Apply resolved Onion source locks to plumOS libretro recipes."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Rewrite libretro recipe refs using docs/onion-libretro-source-lock.tsv."
    )
    parser.add_argument(
        "--recipe-file",
        default=ROOT / "docker/plumos-toolchain/libretro-core-recipes.tsv",
        type=Path,
    )
    parser.add_argument(
        "--lock-file",
        default=ROOT / "docs/onion-libretro-source-lock.tsv",
        type=Path,
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output path. Defaults to stdout.",
    )
    return parser.parse_args()


def read_locks(path: Path) -> dict[str, str]:
    locks: dict[str, str] = {}
    with path.open(newline="") as handle:
        for row in csv.DictReader(handle, dialect="excel-tab"):
            if row.get("resolution_status") != "resolved":
                continue
            commit = row.get("resolved_source_commit", "")
            if len(commit) != 40 or commit == row.get("plumos_ref"):
                continue
            locks[row["id"]] = commit
    return locks


def rewrite_recipes(path: Path, locks: dict[str, str]) -> str:
    lines: list[str] = []
    for raw in path.read_text().splitlines():
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            lines.append(raw)
            continue
        parts = raw.split("|", 6)
        if len(parts) != 7:
            lines.append(raw)
            continue
        core_id = parts[0]
        if core_id in locks:
            parts[3] = locks[core_id]
            lines.append("|".join(parts))
        else:
            lines.append(raw)
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    output = rewrite_recipes(args.recipe_file, read_locks(args.lock_file))
    if args.output:
        args.output.write_text(output)
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
