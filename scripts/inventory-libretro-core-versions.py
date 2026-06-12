#!/usr/bin/env python3
"""Inventory Onion/plumOS libretro core versions and recipe provenance."""

from __future__ import annotations

import argparse
import csv
import hashlib
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


FIELDS = [
    "id",
    "onion_prebuilt",
    "plumos_recipe",
    "plumos_class",
    "version_policy",
    "onion_display_version",
    "onion_corename",
    "onion_binary_sha256",
    "onion_binary_commit",
    "onion_binary_date",
    "onion_binary_subject",
    "onion_builder_file",
    "onion_builder_repo",
    "onion_builder_ref",
    "onion_builder_subdir",
    "onion_builder_makefile",
    "onion_builder_args",
    "plumos_repo",
    "plumos_ref",
    "plumos_subdir",
    "plumos_makefile",
    "plumos_make_args",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Print a TSV inventory of Onion prebuilt libretro core versions, "
            "Onion builder recipes, and plumOS recipes."
        )
    )
    parser.add_argument(
        "--onion-dir",
        default=ROOT / "artifacts/onion/Onion",
        type=Path,
        help="Path to an Onion checkout.",
    )
    parser.add_argument(
        "--builder-dir",
        default=ROOT / "artifacts/onion/Miyoo-Mini-Retroarch-builder",
        type=Path,
        help="Path to a Miyoo-Mini-Retroarch-builder checkout.",
    )
    parser.add_argument(
        "--recipe-file",
        default=ROOT / "docker/plumos-toolchain/libretro-core-recipes.tsv",
        type=Path,
        help="Path to the plumOS libretro recipe file.",
    )
    parser.add_argument(
        "--include-builder-only",
        action="store_true",
        help="Also include Onion builder recipes that are neither Onion prebuilt nor plumOS recipes.",
    )
    return parser.parse_args()


def read_info(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip().strip('"')
    return values


def read_plumos_recipes(path: Path) -> dict[str, dict[str, str]]:
    recipes: dict[str, dict[str, str]] = {}
    if not path.exists():
        return recipes
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|", 6)
        if len(parts) != 7:
            raise ValueError(f"bad recipe line in {path}: {raw}")
        core_id, core_class, repo, ref, subdir, makefile, make_args = parts
        recipes[core_id] = {
            "plumos_class": core_class,
            "plumos_repo": repo,
            "plumos_ref": ref,
            "plumos_subdir": subdir,
            "plumos_makefile": makefile,
            "plumos_make_args": make_args,
        }
    return recipes


def read_builder_recipes(builder_dir: Path) -> dict[str, dict[str, str]]:
    recipes: dict[str, dict[str, str]] = {}
    for recipe_name in ("cores-onionos", "cores-onionos-special"):
        path = builder_dir / recipe_name
        if not path.exists():
            continue
        for raw in path.read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            line = line.split("|", 1)[0].strip()
            parts = line.split()
            if len(parts) < 8:
                continue
            core_id = parts[0]
            subdir = "" if parts[7] == "." else parts[7]
            recipes[core_id] = {
                "onion_builder_file": recipe_name,
                "onion_builder_repo": parts[2],
                "onion_builder_ref": parts[3],
                "onion_builder_subdir": subdir,
                "onion_builder_makefile": parts[6],
                "onion_builder_args": " ".join(parts[8:]),
            }
    return recipes


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_last_change(repo: Path, relative_path: str) -> dict[str, str]:
    if not (repo / ".git").exists():
        return {}
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "log",
            "-1",
            "--format=%H%x09%cI%x09%s",
            "--",
            relative_path,
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    line = result.stdout.strip()
    if not line:
        return {}
    commit, date, subject = (line.split("\t", 2) + ["", ""])[:3]
    return {
        "onion_binary_commit": commit,
        "onion_binary_date": date,
        "onion_binary_subject": subject,
    }


def version_policy(onion_prebuilt: bool, plumos_recipe: bool) -> str:
    if onion_prebuilt and plumos_recipe:
        return "prefer_onion_version"
    if onion_prebuilt:
        return "missing_from_plumos"
    return "plumos_only_latest"


def clean_cell(value: str) -> str:
    cleaned = " ".join(value.replace("\t", " ").splitlines())
    return cleaned if cleaned else "-"


def main() -> int:
    args = parse_args()
    onion_core_dir = args.onion_dir / "static/build/RetroArch/.retroarch/cores"
    if not onion_core_dir.exists():
        raise SystemExit(f"Onion core directory not found: {onion_core_dir}")

    plumos_recipes = read_plumos_recipes(args.recipe_file)
    builder_recipes = read_builder_recipes(args.builder_dir)
    onion_ids = {
        path.name.removesuffix("_libretro.so")
        for path in onion_core_dir.glob("*_libretro.so")
    }

    ids = set(onion_ids) | set(plumos_recipes)
    if args.include_builder_only:
        ids |= set(builder_recipes)

    import sys

    writer = csv.DictWriter(
        sys.stdout,
        fieldnames=FIELDS,
        dialect="excel-tab",
        lineterminator="\n",
    )
    writer.writeheader()

    for core_id in sorted(ids):
        so_path = onion_core_dir / f"{core_id}_libretro.so"
        info_path = onion_core_dir / f"{core_id}_libretro.info"
        has_onion = so_path.exists()
        has_plumos = core_id in plumos_recipes
        info = read_info(info_path)
        row = {field: "" for field in FIELDS}
        row.update(
            {
                "id": core_id,
                "onion_prebuilt": "yes" if has_onion else "no",
                "plumos_recipe": "yes" if has_plumos else "no",
                "version_policy": version_policy(has_onion, has_plumos),
                "onion_display_version": info.get("display_version", ""),
                "onion_corename": info.get("corename", ""),
            }
        )
        if has_onion:
            relative = f"static/build/RetroArch/.retroarch/cores/{so_path.name}"
            row["onion_binary_sha256"] = sha256_file(so_path)
            row.update(git_last_change(args.onion_dir, relative))
        row.update(builder_recipes.get(core_id, {}))
        row.update(plumos_recipes.get(core_id, {}))
        row = {key: clean_cell(value) for key, value in row.items()}
        writer.writerow(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
