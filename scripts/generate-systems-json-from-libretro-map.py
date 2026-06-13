#!/usr/bin/env python3
"""Generate frontend systems.json entries from the libretro system/core map."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any


FIELD_ORDER = [
    "id",
    "display_name",
    "short_name",
    "sort_order",
    "hardware",
    "enabled",
    "pinned",
    "directory_aliases",
    "scan_directories",
    "extensions",
    "artwork",
    "launch_profiles",
    "default_launch_profile",
    "default_cpu_policy",
    "default_cpu_freq_khz",
    "default_cpu_cores",
    "scraper",
]

HARDWARE_BY_ID = {
    "2048": "app",
    "3do": "console",
    "amiga": "computer",
    "atari5200": "console",
    "atari800": "computer",
    "atarist": "computer",
    "bk": "computer",
    "c64": "computer",
    "cannonball": "engine",
    "cavestory": "engine",
    "chailove": "engine",
    "channelf": "console",
    "colecovision": "console",
    "cpc": "computer",
    "daphne": "engine",
    "dinothawr": "engine",
    "flashback": "engine",
    "intellivision": "console",
    "j2me": "app",
    "jaguar": "console",
    "lowresnx": "engine",
    "lutro": "engine",
    "microw8": "engine",
    "mrboom": "engine",
    "music": "app",
    "palm": "portable",
    "pcfx": "console",
    "quake": "engine",
    "rickdangerous": "engine",
    "sg1000": "console",
    "sharpx1": "computer",
    "thomson": "computer",
    "ti83": "app",
    "uzebox": "console",
    "vic20": "computer",
    "vmu": "portable",
    "wolf3d": "engine",
    "zx81": "computer",
    "zxspectrum": "computer",
}

SHORT_NAME_BY_ID = {
    "3do": "3DO",
    "amiga": "Amiga",
    "atari5200": "A5200",
    "atari800": "A800",
    "atarist": "ST",
    "bk": "BK",
    "c64": "C64",
    "cannonball": "OutRun",
    "cavestory": "Cave",
    "chailove": "Chai",
    "channelf": "ChF",
    "colecovision": "Coleco",
    "cpc": "CPC",
    "daphne": "Daphne",
    "dinothawr": "Dino",
    "flashback": "Flashback",
    "intellivision": "INTV",
    "j2me": "J2ME",
    "jaguar": "Jaguar",
    "lowresnx": "LowRes NX",
    "lutro": "Lutro",
    "microw8": "MicroW8",
    "mrboom": "Mr.Boom",
    "music": "Music",
    "palm": "Palm",
    "pcfx": "PC-FX",
    "quake": "Quake",
    "rickdangerous": "XRick",
    "sg1000": "SG-1000",
    "sharpx1": "X1",
    "thomson": "Thomson",
    "ti83": "TI-83",
    "uzebox": "Uzebox",
    "vic20": "VIC-20",
    "vmu": "VMU",
    "wolf3d": "Wolf3D",
    "zx81": "ZX-81",
    "zxspectrum": "Spectrum",
}

ENGINE_IDS = {
    "cannonball",
    "cavestory",
    "daphne",
    "flashback",
    "mrboom",
    "quake",
    "rickdangerous",
    "wolf3d",
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def split_semicolon(value: str) -> list[str]:
    return [part.strip() for part in value.split(";") if part.strip()]


def split_comma(value: str) -> list[str]:
    return [part.strip() for part in value.split(",") if part.strip()]


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ordered_unique(values: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        if value and value not in seen:
            seen.add(value)
            result.append(value)
    return result


def next_alias_priority(system: dict[str, Any]) -> int:
    priorities = [
        int(alias.get("priority", 0))
        for alias in system.get("directory_aliases", [])
        if isinstance(alias, dict)
    ]
    return (max(priorities) if priorities else 0) + 10


def alias_source(name: str, notes: str, existing_system: bool) -> str:
    if "Onion folder" in notes:
        return "onion"
    if existing_system:
        return "miyoo" if name.upper() == name else "rocknix"
    return "plumos"


def add_directory_aliases(system: dict[str, Any], row: dict[str, str], existing_system: bool) -> None:
    aliases = system.setdefault("directory_aliases", [])
    existing_names = {
        str(alias.get("name", ""))
        for alias in aliases
        if isinstance(alias, dict) and alias.get("name")
    }
    priority = next_alias_priority(system)
    notes = row.get("notes", "")
    for name in ordered_unique([row.get("rom_dir", "")] + split_semicolon(row.get("directory_aliases", ""))):
        if name in existing_names:
            continue
        aliases.append(
            {
                "name": name,
                "source": alias_source(name, notes, existing_system),
                "priority": priority,
            }
        )
        priority += 10
        existing_names.add(name)


def add_extensions(system: dict[str, Any], row: dict[str, str]) -> None:
    system["extensions"] = ordered_unique(
        list(system.get("extensions", [])) + split_comma(row.get("extensions", ""))
    )


def add_launch_profiles(system: dict[str, Any], row: dict[str, str]) -> None:
    profiles = list(system.get("launch_profiles", []))
    for core in split_comma(row.get("candidate_cores", "")):
        profile = f"retroarch:{core}"
        if profile not in profiles:
            profiles.append(profile)
    default_core = row.get("default_core", "").strip()
    default_profile = f"retroarch:{default_core}" if default_core else ""
    if default_profile and default_profile not in profiles:
        profiles.append(default_profile)
    system["launch_profiles"] = profiles


def scraper_policy(system_id: str, hardware: str, status: str) -> dict[str, Any]:
    if status == "contentless":
        return {"enabled": False, "reason": "contentless_core"}
    if hardware == "engine" or system_id in ENGINE_IDS:
        return {"enabled": False, "reason": "data_layout_policy_pending"}
    if system_id in {"j2me", "music", "palm", "ti83", "vmu"}:
        return {"enabled": False, "reason": "frontend_policy_pending"}
    return {"enabled": False, "reason": "scraper_source_pending"}


def new_system(row: dict[str, str], sort_order: int) -> dict[str, Any]:
    system_id = row["system_id"]
    hardware = HARDWARE_BY_ID.get(system_id, "console")
    default_core = row.get("default_core", "").strip()
    system: dict[str, Any] = {
        "id": system_id,
        "display_name": row["display_name"],
        "short_name": SHORT_NAME_BY_ID.get(system_id, row["display_name"]),
        "sort_order": sort_order,
        "hardware": hardware,
        "enabled": True,
        "pinned": row.get("status", "") == "contentless",
        "directory_aliases": [],
        "extensions": split_comma(row.get("extensions", "")),
        "artwork": {"lookup": [{"root": "sdcard", "path": f"Images/{system_id}"}]},
        "launch_profiles": [],
        "default_launch_profile": f"retroarch:{default_core}" if default_core else "",
        "default_cpu_policy": "fixed",
        "default_cpu_freq_khz": 648000,
        "scraper": scraper_policy(system_id, hardware, row.get("status", "")),
    }
    add_directory_aliases(system, row, existing_system=False)
    add_launch_profiles(system, row)
    return system


def update_systems(config: dict[str, Any], rows: list[dict[str, str]]) -> tuple[int, int, int]:
    systems = config.setdefault("systems", [])
    by_id = {system.get("id"): system for system in systems}
    max_sort = max((int(system.get("sort_order", 0)) for system in systems), default=0)
    added_systems = 0
    updated_systems = 0
    added_profiles = 0

    for row in rows:
        system_id = row["system_id"]
        if system_id in by_id:
            system = by_id[system_id]
            before_system = json.dumps(system, sort_keys=True)
            before_profiles = set(system.get("launch_profiles", []))
            if row.get("status", "") == "contentless":
                system["pinned"] = True
            add_directory_aliases(system, row, existing_system=True)
            add_extensions(system, row)
            add_launch_profiles(system, row)
            after_profiles = set(system.get("launch_profiles", []))
            added_profiles += len(after_profiles - before_profiles)
            if row.get("display_name") and not system.get("display_name"):
                system["display_name"] = row["display_name"]
            if row.get("display_name") and not system.get("short_name"):
                system["short_name"] = row["display_name"]
            if json.dumps(system, sort_keys=True) != before_system:
                updated_systems += 1
            continue

        max_sort += 10
        system = new_system(row, max_sort)
        systems.append(system)
        by_id[system_id] = system
        added_systems += 1
        added_profiles += len(system.get("launch_profiles", []))

    return added_systems, updated_systems, added_profiles


def dumps_inline(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(", ", ": "))


def format_aliases(aliases: list[dict[str, Any]], indent: str) -> list[str]:
    lines = [f'{indent}"directory_aliases": [']
    for i, alias in enumerate(aliases):
        comma = "," if i + 1 < len(aliases) else ""
        lines.append(f"{indent}  {dumps_inline(alias)}{comma}")
    lines.append(f"{indent}]")
    return lines


def format_artwork(artwork: dict[str, Any], indent: str) -> list[str]:
    lookups = artwork.get("lookup", [])
    lines = [f'{indent}"artwork": {{', f'{indent}  "lookup": [']
    for i, lookup in enumerate(lookups):
        comma = "," if i + 1 < len(lookups) else ""
        lines.append(f"{indent}    {dumps_inline(lookup)}{comma}")
    lines.extend([f"{indent}  ]", f"{indent}}}"])
    return lines


def format_scraper(scraper: dict[str, Any], indent: str) -> list[str]:
    lines = [f'{indent}"scraper": {{']
    items = list(scraper.items())
    for i, (key, value) in enumerate(items):
        comma = "," if i + 1 < len(items) else ""
        lines.append(f'{indent}  "{key}": {dumps_inline(value)}{comma}')
    lines.append(f"{indent}}}")
    return lines


def format_field(key: str, value: Any, indent: str) -> list[str]:
    if key == "directory_aliases":
        return format_aliases(value, indent)
    if key == "artwork":
        return format_artwork(value, indent)
    if key == "scraper" and isinstance(value, dict):
        return format_scraper(value, indent)
    return [f'{indent}"{key}": {dumps_inline(value)}']


def format_system(system: dict[str, Any], last: bool) -> list[str]:
    keys = [key for key in FIELD_ORDER if key in system]
    keys.extend(key for key in system.keys() if key not in FIELD_ORDER)
    lines = ["    {"]
    for idx, key in enumerate(keys):
        field_lines = format_field(key, system[key], "      ")
        if idx + 1 < len(keys):
            field_lines[-1] += ","
        lines.extend(field_lines)
    lines.append("    }" + ("" if last else ","))
    return lines


def write_config(config: dict[str, Any], path: Path) -> None:
    systems = config.get("systems", [])
    lines = ["{", f'  "version": {dumps_inline(config.get("version", 1))},', '  "systems": [']
    for i, system in enumerate(systems):
        lines.extend(format_system(system, i + 1 == len(systems)))
    lines.extend(["  ]", "}", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--systems",
        type=Path,
        default=root / "package/frontend/plumos/config/frontend/systems.json",
        help="systems.json to update.",
    )
    parser.add_argument(
        "--map",
        type=Path,
        default=root / "docs/libretro-system-core-map.tsv",
        help="libretro system/core TSV map.",
    )
    parser.add_argument("--check", action="store_true", help="Validate without writing.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = read_json(args.systems)
    rows = read_tsv(args.map)
    added_systems, updated_systems, added_profiles = update_systems(config, rows)

    systems = config.get("systems", [])
    system_ids = [system.get("id", "") for system in systems]
    duplicate_ids = sorted({system_id for system_id in system_ids if system_ids.count(system_id) > 1})
    max_extensions = max((len(system.get("extensions", [])) for system in systems), default=0)
    max_profiles = max((len(system.get("launch_profiles", [])) for system in systems), default=0)
    if duplicate_ids:
        raise SystemExit(f"duplicate system ids: {', '.join(duplicate_ids)}")

    if not args.check:
        write_config(config, args.systems)

    print(
        "systems={systems} added_systems={added_systems} updated_systems={updated_systems} "
        "added_profiles={added_profiles} max_extensions={max_extensions} max_profiles={max_profiles}".format(
            systems=len(systems),
            added_systems=added_systems,
            updated_systems=updated_systems,
            added_profiles=added_profiles,
            max_extensions=max_extensions,
            max_profiles=max_profiles,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
