#!/usr/bin/env python3
"""Audit Graphic system logo PNGs against systems.json."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


SOURCE_SHEET_NAMES = {"all-logo.png"}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_systems_path(root: Path) -> Path:
    return root / "package/frontend/plumos/config/frontend/systems.json"


def default_logo_dir(root: Path) -> Path:
    return root / "package/frontend/plumos/themes/default/logos/systems"


def load_systems(path: Path, include_disabled: bool) -> tuple[list[str], list[str]]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)

    targets: list[str] = []
    disabled: list[str] = []
    for system in data.get("systems", []):
        system_id = str(system.get("id", "")).strip()
        if not system_id:
            continue
        if system.get("enabled") is False and not include_disabled:
            disabled.append(system_id)
            continue
        targets.append(system_id)
    return targets, disabled


def load_logo_ids(path: Path) -> list[str]:
    if not path.is_dir():
        return []
    ids: list[str] = []
    for item in path.iterdir():
        if not item.is_file() or item.suffix.lower() != ".png":
            continue
        if item.name in SOURCE_SHEET_NAMES:
            continue
        ids.append(item.stem)
    return sorted(ids)


def csv(items: list[str]) -> str:
    return ",".join(items) if items else "-"


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description="Audit Graphic system logo PNGs against enabled systems."
    )
    parser.add_argument(
        "--systems-json",
        type=Path,
        default=default_systems_path(root),
        help="systems.json path. Default: package frontend systems.json.",
    )
    parser.add_argument(
        "--logo-dir",
        type=Path,
        default=default_logo_dir(root),
        help="Theme system logo directory. Default: default theme logos/systems.",
    )
    parser.add_argument(
        "--include-disabled",
        action="store_true",
        help="Treat systems with enabled:false as required logo targets.",
    )
    parser.add_argument(
        "--list-targets",
        action="store_true",
        help="Print one required system id per line instead of summary output.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when required logos are missing or extra logos exist.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    target_ids, disabled_ids = load_systems(args.systems_json, args.include_disabled)
    logo_ids = load_logo_ids(args.logo_dir)

    target_set = set(target_ids)
    logo_set = set(logo_ids)
    missing = sorted(target_set - logo_set)
    disabled_set = set(disabled_ids)
    extra = sorted(logo_set - target_set - disabled_set)
    disabled_with_logo = sorted(set(disabled_ids) & logo_set)

    if args.list_targets:
        for system_id in target_ids:
            print(system_id)
        return 0

    print(f"target_systems={len(target_ids)}")
    print(f"logo_pngs={len(logo_ids)}")
    print(f"disabled_excluded={csv(sorted(disabled_ids))}")
    print(f"missing_logo={csv(missing)}")
    print(f"extra_logo={csv(extra)}")
    print(f"disabled_with_logo={csv(disabled_with_logo)}")

    if args.strict and (missing or extra):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
