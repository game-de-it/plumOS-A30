#!/usr/bin/env python3
"""Generate FE verification target matrices from build and device inventories."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


PICOARCH_BLOCKED_CORES = {
    "easyrpg",
    "freeintv",
    "mame2003_plus",
    "mednafen_pce",
    "nekop2",
    "nxengine",
    "np2kai",
    "quasi88",
    "retro8",
    "squirreljme",
    "tgbdual",
}

STANDALONE_BINARIES = {
    "dosbox": ["/mnt/SDCARD/plumos/emulators/dosbox-staging/bin/dosbox"],
    "dosbox-staging": ["/mnt/SDCARD/plumos/emulators/dosbox-staging/bin/dosbox"],
    "easyrpg": ["/mnt/SDCARD/plumos/emulators/easyrpg/bin/easyrpg-player"],
    "openbor": ["/mnt/SDCARD/plumos/emulators/openbor/bin/OpenBOR"],
    "pcsx": ["/mnt/SDCARD/plumos/emulators/pcsx_rearmed/bin/pcsx"],
    "pcsx_rearmed": ["/mnt/SDCARD/plumos/emulators/pcsx_rearmed/bin/pcsx"],
    "ppsspp": ["/mnt/SDCARD/plumos/emulators/ppsspp/bin/PPSSPPSDL"],
    "ppsspp-display-ui": ["/mnt/SDCARD/plumos/emulators/ppsspp-display-ui/bin/PPSSPPSDL"],
    "ppsspp-vanilla": ["/mnt/SDCARD/plumos/emulators/ppsspp-vanilla/bin/PPSSPPSDL"],
    "red-viper": [
        "/mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-sdlgl-a30",
        "/mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-a30",
    ],
    "red_viper": [
        "/mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-sdlgl-a30",
        "/mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-a30",
    ],
    "scummvm": ["/mnt/SDCARD/plumos/emulators/scummvm/bin/scummvm"],
}

NON_LIBRETRO_LAUNCHERS = {
    "pyxel:a30": "/mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch",
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def yes_no(value: bool) -> str:
    return "yes" if value else "no"


def read_lines(path: Path) -> set[str]:
    if not path or not path.exists():
        return set()
    values: set[str] = set()
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            value = line.strip()
            if not value or value.startswith("#"):
                continue
            if value.endswith("_libretro.so"):
                value = value.removesuffix("_libretro.so")
            values.add(value)
    return values


def read_built_cores(path: Path) -> set[str]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return {
            row["core_id"]
            for row in csv.DictReader(handle, delimiter="\t")
            if row.get("core_id")
        }


def load_systems(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    return sorted(
        list(data.get("systems", [])),
        key=lambda system: str(system.get("id", "")).lower(),
    )


def system_rom_dirs(system: dict) -> str:
    aliases = system.get("directory_aliases", [])
    names = [str(alias.get("name", "")) for alias in aliases if alias.get("name")]
    return ";".join(names)


def system_extensions(system: dict) -> str:
    return ";".join(str(ext) for ext in system.get("extensions", []) if ext)


def load_runtime_records(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    if not path.exists():
        return {}
    records: dict[tuple[str, str], dict[str, str]] = {}
    with path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            system_id = row.get("system_id", "")
            profile = row.get("launch_profile", "") or row.get("profile", "")
            status = row.get("status", "")
            if system_id and profile and status:
                records[(system_id, profile)] = {
                    "status": status,
                    "notes": row.get("notes", ""),
                }
    return records


def load_previous_records(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    if not path.exists():
        return {}
    records: dict[tuple[str, str], dict[str, str]] = {}
    with path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            system_id = row.get("system_id", "")
            profile = row.get("launch_profile", "")
            if system_id and profile:
                records[(system_id, profile)] = {
                    "status": row.get("verification_status", ""),
                    "notes": row.get("notes", ""),
                }
    return records


def verification_status(
    runtime_records: dict[tuple[str, str], dict[str, str]],
    system_id: str,
    profile: str,
) -> str:
    return runtime_records.get((system_id, profile), {}).get("status", "untracked")


def row_notes(
    source: str,
    system_id: str,
    profile: str,
    status: str,
    runtime_records: dict[tuple[str, str], dict[str, str]],
    previous_records: dict[tuple[str, str], dict[str, str]],
) -> list[str]:
    key = (system_id, profile)
    previous = previous_records.get(key, {})
    previous_notes = previous.get("notes", "")
    previous_status = previous.get("status", "")
    runtime_notes = runtime_records.get(key, {}).get("notes", "")

    if previous_status == status and previous_notes and ";" in previous_notes:
        return [previous_notes]
    if previous_status and previous_status != status and runtime_notes:
        return [f"{source}; {runtime_notes}"]
    return [source]


def add_profile(profiles: list[str], profile: str) -> None:
    if profile and profile not in profiles:
        profiles.append(profile)


def libretro_rows(
    systems: list[dict],
    built_cores: set[str],
    deployed_cores: set[str],
    deployed_paths: set[str],
    runtime_records: dict[tuple[str, str], dict[str, str]],
    previous_records: dict[tuple[str, str], dict[str, str]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    picoarch_deployed = "/mnt/SDCARD/plumos/bin/plumos-picoarch-launch" in deployed_paths

    for system in systems:
        if system.get("enabled") is False:
            continue
        system_id = str(system.get("id", ""))
        display_name = str(system.get("display_name", system_id))
        default_profile = str(system.get("default_launch_profile", ""))
        profiles: list[str] = []
        for profile in system.get("launch_profiles", []):
            add_profile(profiles, str(profile))
        add_profile(profiles, default_profile)

        retroarch_profiles = [profile for profile in profiles if profile.startswith("retroarch:")]
        picoarch_profiles = [profile for profile in profiles if profile.startswith("picoarch:")]
        for profile in retroarch_profiles:
            add_libretro_row(
                rows,
                system,
                system_id,
                display_name,
                default_profile,
                "RA",
                profile,
                profile.split(":", 1)[1],
                built_cores,
                deployed_cores,
                True,
                runtime_records,
                previous_records,
                "systems.json",
            )

        for profile in picoarch_profiles:
            add_libretro_row(
                rows,
                system,
                system_id,
                display_name,
                default_profile,
                "PICO",
                profile,
                profile.split(":", 1)[1],
                built_cores,
                deployed_cores,
                picoarch_deployed,
                runtime_records,
                previous_records,
                "systems.json",
            )

        for profile in retroarch_profiles:
            core_id = profile.split(":", 1)[1]
            if core_id in PICOARCH_BLOCKED_CORES:
                continue
            pico_profile = f"picoarch:{core_id}"
            if pico_profile in picoarch_profiles:
                continue
            add_libretro_row(
                rows,
                system,
                system_id,
                display_name,
                default_profile,
                "PICO",
                pico_profile,
                core_id,
                built_cores,
                deployed_cores,
                picoarch_deployed,
                runtime_records,
                previous_records,
                "FE auto companion",
            )

    return rows


def add_libretro_row(
    rows: list[dict[str, str]],
    system: dict,
    system_id: str,
    display_name: str,
    default_profile: str,
    runtime: str,
    profile: str,
    core_id: str,
    built_cores: set[str],
    deployed_cores: set[str],
    runtime_deployed: bool,
    runtime_records: dict[tuple[str, str], dict[str, str]],
    previous_records: dict[tuple[str, str], dict[str, str]],
    source: str,
) -> None:
    built = core_id in built_cores
    deployed = core_id in deployed_cores
    fe_executable = runtime_deployed and deployed
    status = verification_status(runtime_records, system_id, profile)
    target = built and deployed and fe_executable and status != "retired"
    notes = row_notes(source, system_id, profile, status, runtime_records, previous_records)
    if not built:
        notes.append("missing_plumos_build")
    if not deployed:
        notes.append("missing_on_device")
    if runtime == "PICO" and not runtime_deployed:
        notes.append("missing_picoarch_launcher")
    rows.append(
        {
            "system_id": system_id,
            "display_name": display_name,
            "rom_dirs": system_rom_dirs(system),
            "extensions": system_extensions(system),
            "runtime": runtime,
            "launch_profile": profile,
            "core_id": core_id,
            "built_core": yes_no(built),
            "deployed_core": yes_no(deployed),
            "fe_selectable": "yes",
            "fe_executable": yes_no(fe_executable),
            "target_for_verification": yes_no(target),
            "default_profile": yes_no(profile == default_profile),
            "verification_status": status,
            "notes": ";".join(notes),
        }
    )


def non_libretro_rows(
    systems: list[dict],
    deployed_paths: set[str],
    runtime_records: dict[tuple[str, str], dict[str, str]],
    previous_records: dict[tuple[str, str], dict[str, str]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    standalone_launcher = "/mnt/SDCARD/plumos/bin/plumos-standalone-launch" in deployed_paths

    for system in systems:
        if system.get("enabled") is False:
            continue
        system_id = str(system.get("id", ""))
        display_name = str(system.get("display_name", system_id))
        default_profile = str(system.get("default_launch_profile", ""))
        profiles: list[str] = []
        for profile in system.get("launch_profiles", []):
            add_profile(profiles, str(profile))
        add_profile(profiles, default_profile)
        for profile in profiles:
            if profile.startswith("standalone:"):
                emulator_id = profile.split(":", 1)[1]
                candidates = STANDALONE_BINARIES.get(emulator_id, [])
                runtime_binary = any(path in deployed_paths for path in candidates)
                fe_executable = standalone_launcher
                status = verification_status(runtime_records, system_id, profile)
                target = fe_executable and runtime_binary and status != "retired"
                notes = row_notes(
                    "systems.json",
                    system_id,
                    profile,
                    status,
                    runtime_records,
                    previous_records,
                )
                rows.append(
                    {
                        "system_id": system_id,
                        "display_name": display_name,
                        "rom_dirs": system_rom_dirs(system),
                        "extensions": system_extensions(system),
                        "runtime": "SA",
                        "launch_profile": profile,
                        "emulator_id": emulator_id,
                        "launcher_deployed": yes_no(standalone_launcher),
                        "runtime_binary_deployed": yes_no(runtime_binary),
                        "fe_selectable": "yes",
                        "fe_executable": yes_no(fe_executable),
                        "target_for_verification": yes_no(target),
                        "default_profile": yes_no(profile == default_profile),
                        "verification_status": status,
                        "notes": ";".join(notes),
                    }
                )
            elif profile in NON_LIBRETRO_LAUNCHERS:
                launcher = NON_LIBRETRO_LAUNCHERS[profile]
                deployed = launcher in deployed_paths
                status = verification_status(runtime_records, system_id, profile)
                notes = row_notes(
                    "systems.json",
                    system_id,
                    profile,
                    status,
                    runtime_records,
                    previous_records,
                )
                rows.append(
                    {
                        "system_id": system_id,
                        "display_name": display_name,
                        "rom_dirs": system_rom_dirs(system),
                        "extensions": system_extensions(system),
                        "runtime": "PYXEL",
                        "launch_profile": profile,
                        "emulator_id": profile.split(":", 1)[1],
                        "launcher_deployed": yes_no(deployed),
                        "runtime_binary_deployed": yes_no(deployed),
                        "fe_selectable": "yes",
                        "fe_executable": yes_no(deployed),
                        "target_for_verification": yes_no(deployed and status != "retired"),
                        "default_profile": yes_no(profile == default_profile),
                        "verification_status": status,
                        "notes": ";".join(notes),
                    }
                )

    return rows


def write_tsv(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, delimiter="\t", fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description="Generate FE verification target TSV files from build/deploy inventories."
    )
    parser.add_argument(
        "--systems",
        default=str(root / "package/frontend/plumos/config/frontend/systems.json"),
    )
    parser.add_argument("--built-cores", default=str(root / "docs/libretro-built-cores.tsv"))
    parser.add_argument("--deployed-cores", required=True)
    parser.add_argument("--deployed-paths", required=True)
    parser.add_argument(
        "--runtime-status",
        default=str(root / "docs/emulator-runtime-verification.tsv"),
    )
    parser.add_argument(
        "--libretro-out",
        default=str(root / "docs/emulator-fe-libretro-targets.tsv"),
    )
    parser.add_argument(
        "--standalone-out",
        default=str(root / "docs/emulator-fe-standalone-targets.tsv"),
    )
    parser.add_argument(
        "--previous-libretro",
        default=None,
        help="Existing libretro FE TSV to preserve unchanged-row notes from.",
    )
    parser.add_argument(
        "--previous-standalone",
        default=None,
        help="Existing standalone FE TSV to preserve unchanged-row notes from.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    systems = load_systems(Path(args.systems))
    built_cores = read_built_cores(Path(args.built_cores))
    deployed_cores = read_lines(Path(args.deployed_cores))
    deployed_paths = read_lines(Path(args.deployed_paths))
    runtime_records = load_runtime_records(Path(args.runtime_status))

    libretro_out = Path(args.libretro_out)
    standalone_out = Path(args.standalone_out)
    previous_libretro = load_previous_records(
        Path(args.previous_libretro) if args.previous_libretro else libretro_out
    )
    previous_standalone = load_previous_records(
        Path(args.previous_standalone) if args.previous_standalone else standalone_out
    )

    libretro = libretro_rows(
        systems,
        built_cores,
        deployed_cores,
        deployed_paths,
        runtime_records,
        previous_libretro,
    )
    standalone = non_libretro_rows(
        systems,
        deployed_paths,
        runtime_records,
        previous_standalone,
    )

    write_tsv(
        libretro_out,
        libretro,
        [
            "system_id",
            "display_name",
            "rom_dirs",
            "extensions",
            "runtime",
            "launch_profile",
            "core_id",
            "built_core",
            "deployed_core",
            "fe_selectable",
            "fe_executable",
            "target_for_verification",
            "default_profile",
            "verification_status",
            "notes",
        ],
    )
    write_tsv(
        standalone_out,
        standalone,
        [
            "system_id",
            "display_name",
            "rom_dirs",
            "extensions",
            "runtime",
            "launch_profile",
            "emulator_id",
            "launcher_deployed",
            "runtime_binary_deployed",
            "fe_selectable",
            "fe_executable",
            "target_for_verification",
            "default_profile",
            "verification_status",
            "notes",
        ],
    )

    print(
        "generated libretro_rows={libretro} standalone_rows={standalone} "
        "built_cores={built} deployed_cores={deployed}".format(
            libretro=len(libretro),
            standalone=len(standalone),
            built=len(built_cores),
            deployed=len(deployed_cores),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
