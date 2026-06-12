#!/usr/bin/env python3
"""Build plumOS thumbnail rescue overlays from verified seed rows.

The generated runtime data is intentionally small:

  rescue/<system>/<kind>.tsv
    crc<TAB>href<TAB>thumbnail_label<TAB>source

The raw seed rows are not used directly by the A30 runtime.  A row is emitted
only when its CRC is not already covered by the libretro base CRC index and the
seed thumbnail label exists in the libretro thumbnail index for that kind.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def read_enabled_systems(systems_json: Path) -> set[str]:
    with systems_json.open(encoding="utf-8") as handle:
        data = json.load(handle)
    enabled: set[str] = set()
    for system in data.get("systems", []):
        scraper = system.get("scraper") or {}
        if scraper.get("enabled") is True:
            enabled.add(str(system.get("id", "")))
    return enabled


def read_source_systems(sources_tsv: Path) -> set[str]:
    systems: set[str] = set()
    with sources_tsv.open(encoding="utf-8") as handle:
        for row in csv.reader(handle, delimiter="\t"):
            if not row or not row[0] or row[0].startswith("#"):
                continue
            systems.add(row[0])
    return systems


def read_source_dat_basenames(sources_tsv: Path) -> dict[str, str]:
    basenames: dict[str, str] = {}
    with sources_tsv.open(encoding="utf-8") as handle:
        for row in csv.reader(handle, delimiter="\t"):
            if len(row) < 3 or not row[0] or row[0].startswith("#"):
                continue
            basenames[row[0]] = Path(row[2]).stem
    return basenames


def read_base_crcs(cache_root: Path, system: str) -> set[str]:
    path = cache_root / "dat" / f"{system}.crc.tsv"
    crcs: set[str] = set()
    if not path.exists():
        return crcs
    with path.open(encoding="utf-8", errors="replace") as handle:
        for row in csv.reader(handle, delimiter="\t"):
            if row and row[0]:
                crcs.add(row[0].lower())
    return crcs


def read_thumb_index(cache_root: Path, system: str, kind: str) -> dict[str, str]:
    path = cache_root / "thumb-names" / system / f"{kind}.tsv"
    index: dict[str, str] = {}
    if not path.exists():
        return index
    with path.open(encoding="utf-8", errors="replace") as handle:
        for row in csv.reader(handle, delimiter="\t"):
            if len(row) >= 2 and row[0] and row[1]:
                index[row[0]] = row[1]
    return index


def read_seeds(seeds_tsv: Path) -> list[dict[str, str]]:
    seeds: list[dict[str, str]] = []
    if not seeds_tsv.exists():
        return seeds
    with seeds_tsv.open(encoding="utf-8", errors="replace", newline="") as handle:
        for line_no, row in enumerate(csv.reader(handle, delimiter="\t"), start=1):
            if not row or not row[0] or row[0].startswith("#"):
                continue
            if len(row) < 4:
                raise SystemExit(f"{seeds_tsv}:{line_no}: expected system, crc, thumbnail_label, source")
            system, crc, label, source = row[:4]
            crc = crc.lower()
            if len(crc) != 8 or any(ch not in "0123456789abcdef" for ch in crc):
                raise SystemExit(f"{seeds_tsv}:{line_no}: invalid crc: {crc}")
            seeds.append({
                "system": system,
                "crc": crc,
                "thumbnail_label": label,
                "source": source,
            })
    return seeds


def parse_clrmamepro_dat(path: Path) -> list[tuple[str, str]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    rows: list[tuple[str, str]] = []
    for block in re.findall(r"game\s*\((.*?)\n\)", text, flags=re.S):
        name_match = re.search(r'^\s*name\s+"(.*)"', block, flags=re.M)
        if not name_match:
            continue
        name = name_match.group(1)
        for crc in re.findall(r"\bcrc\s+([0-9A-Fa-f]{8})\b", block):
            rows.append((crc.lower(), name))
    return rows


def external_dat_paths(root: Path, dat_basename: str) -> list[Path]:
    if not root.exists():
        return []
    matches: list[Path] = []
    for path in root.rglob("*.dat"):
        if dat_basename.casefold() in path.stem.casefold():
            matches.append(path)
    return sorted(matches)


def thumbnail_label_candidates(name: str) -> list[str]:
    replacements = [
        (" (En)", ""),
        (" (Ja)", ""),
        (" (En,Ja)", ""),
        (" (Ja,En)", ""),
        (" (Fr)", ""),
        (" (De)", ""),
        (" (Es)", ""),
        (" (It)", ""),
        (" (Nl)", ""),
        (" (Sv)", ""),
        (" (Pt)", ""),
        (" (Ru)", ""),
        (" (Ko)", ""),
        (" (Zh)", ""),
    ]
    candidates = [name]
    stripped = name
    for old, new in replacements:
        stripped = stripped.replace(old, new)
    candidates.append(stripped)
    candidates.append(re.sub(r" \(Rev [^)]+\)", "", stripped))
    candidates.append(re.sub(r" \((Beta|Proto|Sample|Program|Aftermarket|Unl|Virtual Console)[^)]*\)", "", stripped))

    seen: set[str] = set()
    unique: list[str] = []
    for candidate in candidates:
        if candidate and candidate not in seen:
            unique.append(candidate)
            seen.add(candidate)
    return unique


def read_external_dat_seeds(
    roots: list[Path],
    dat_basenames: dict[str, str],
    allowed_systems: set[str],
) -> list[dict[str, str]]:
    seeds: list[dict[str, str]] = []
    for system in sorted(allowed_systems):
        dat_basename = dat_basenames.get(system)
        if not dat_basename:
            continue
        for root in roots:
            for dat_path in external_dat_paths(root, dat_basename):
                for crc, label in parse_clrmamepro_dat(dat_path):
                    seeds.append({
                        "system": system,
                        "crc": crc,
                        "thumbnail_label": label,
                        "source": f"external-dat:{dat_path.name}",
                    })
    return seeds


def write_overlay(path: Path, rows: list[tuple[str, str, str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        for row in sorted(rows, key=lambda item: item[0]):
            writer.writerow(row)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--systems-json", type=Path, required=True)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--cache-root", type=Path, required=True)
    parser.add_argument("--seeds", type=Path, required=True)
    parser.add_argument("--external-dat-root", action="append", type=Path, default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kind", action="append", dest="kinds")
    args = parser.parse_args()

    kinds = args.kinds or ["Named_Boxarts", "Named_Snaps", "Named_Titles"]
    enabled = read_enabled_systems(args.systems_json)
    source_systems = read_source_systems(args.sources)
    dat_basenames = read_source_dat_basenames(args.sources)
    allowed_systems = enabled & source_systems
    seed_rows = read_seeds(args.seeds)
    seed_rows.extend(read_external_dat_seeds(args.external_dat_root, dat_basenames, allowed_systems))
    seeds = [seed for seed in seed_rows if seed["system"] in allowed_systems]
    by_system: dict[str, list[dict[str, str]]] = defaultdict(list)
    for seed in seeds:
        by_system[seed["system"]].append(seed)

    totals = {
        "seed_rows": len(seeds),
        "written_rows": 0,
        "base_duplicate": 0,
        "thumbnail_missing": 0,
    }
    manifest_lines = [
        "thumbnail rescue overlays",
        f"generated_at={datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        f"seeds={args.seeds.name}",
        f"kinds={' '.join(kinds)}",
    ]

    for system in sorted(allowed_systems):
        system_seeds = by_system.get(system, [])
        base_crcs = read_base_crcs(args.cache_root, system)
        for kind in kinds:
            thumb_index = read_thumb_index(args.cache_root, system, kind)
            rows: list[tuple[str, str, str, str]] = []
            seen_rescue_crcs: set[str] = set()
            for seed in system_seeds:
                if seed["crc"] in base_crcs:
                    totals["base_duplicate"] += 1
                    continue
                if seed["crc"] in seen_rescue_crcs:
                    continue
                href = ""
                matched_label = seed["thumbnail_label"]
                for candidate in thumbnail_label_candidates(seed["thumbnail_label"]):
                    href = thumb_index.get(candidate, "")
                    if href:
                        matched_label = candidate
                        break
                if not href:
                    totals["thumbnail_missing"] += 1
                    continue
                rows.append((seed["crc"], href, matched_label, seed["source"]))
                seen_rescue_crcs.add(seed["crc"])
            if rows:
                output_path = args.output / system / f"{kind}.tsv"
                write_overlay(output_path, rows)
                totals["written_rows"] += len(rows)
                print(f"rescue_overlay\t{system}\t{kind}\trows={len(rows)}\t{output_path}")

    manifest_lines.extend(f"{key}={value}" for key, value in totals.items())
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.txt").write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
    print(
        "rescue_summary\t"
        f"seed_rows={totals['seed_rows']}\t"
        f"written_rows={totals['written_rows']}\t"
        f"base_duplicate={totals['base_duplicate']}\t"
        f"thumbnail_missing={totals['thumbnail_missing']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
