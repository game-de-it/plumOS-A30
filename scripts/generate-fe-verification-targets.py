#!/usr/bin/env python3
"""Generate FE verification target matrices from build and device inventories."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


PICOARCH_BLOCKED_CORES = {
    "mednafen_pce",
    "quasi88",
    "tgbdual",
}

STANDALONE_BINARIES = {
    "dosbox": ["/mnt/SDCARD/plumos/emulators/dosbox-staging/bin/dosbox"],
    "dosbox-staging": ["/mnt/SDCARD/plumos/emulators/dosbox-staging/bin/dosbox"],
    "easyrpg": ["/mnt/SDCARD/plumos/emulators/easyrpg/bin/easyrpg-player"],
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
    return list(data.get("systems", []))


def system_rom_dirs(system: dict) -> str:
    aliases = system.get("directory_aliases", [])
    names = [str(alias.get("name", "")) for alias in aliases if alias.get("name")]
    return ";".join(names)


def system_extensions(system: dict) -> str:
    return ";".join(str(ext) for ext in system.get("extensions", []) if ext)


def load_statuses(path: Path) -> dict[tuple[str, str], str]:
    if not path.exists():
        return {}
    statuses: dict[tuple[str, str], str] = {}
    with path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            system_id = row.get("system_id", "")
            profile = row.get("launch_profile", "") or row.get("profile", "")
            status = row.get("status", "")
            if system_id and profile and status:
                statuses[(system_id, profile)] = status
    return statuses


def add_profile(profiles: list[str], profile: str) -> None:
    if profile and profile not in profiles:
        profiles.append(profile)


def libretro_rows(
    systems: list[dict],
    built_cores: set[str],
    deployed_cores: set[str],
    deployed_paths: set[str],
    statuses: dict[tuple[str, str], str],
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
                statuses,
                "systems.json",
            )

        for profile in retroarch_profiles:
            core_id = profile.split(":", 1)[1]
            if core_id in PICOARCH_BLOCKED_CORES:
                continue
            pico_profile = f"picoarch:{core_id}"
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
                statuses,
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
    statuses: dict[tuple[str, str], str],
    source: str,
) -> None:
    built = core_id in built_cores
    deployed = core_id in deployed_cores
    fe_executable = runtime_deployed and deployed
    target = built and deployed and fe_executable
    notes: list[str] = [source]
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
            "verification_status": statuses.get((system_id, profile), "untracked"),
            "notes": ";".join(notes),
        }
    )


def non_libretro_rows(
    systems: list[dict],
    deployed_paths: set[str],
    statuses: dict[tuple[str, str], str],
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
                target = fe_executable and runtime_binary
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
                        "verification_status": statuses.get((system_id, profile), "untracked"),
                        "notes": "systems.json",
                    }
                )
            elif profile in NON_LIBRETRO_LAUNCHERS:
                launcher = NON_LIBRETRO_LAUNCHERS[profile]
                deployed = launcher in deployed_paths
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
                        "target_for_verification": yes_no(deployed),
                        "default_profile": yes_no(profile == default_profile),
                        "verification_status": statuses.get((system_id, profile), "untracked"),
                        "notes": "systems.json",
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
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    systems = load_systems(Path(args.systems))
    built_cores = read_built_cores(Path(args.built_cores))
    deployed_cores = read_lines(Path(args.deployed_cores))
    deployed_paths = read_lines(Path(args.deployed_paths))
    statuses = load_statuses(Path(args.runtime_status))

    libretro = libretro_rows(systems, built_cores, deployed_cores, deployed_paths, statuses)
    standalone = non_libretro_rows(systems, deployed_paths, statuses)

    write_tsv(
        Path(args.libretro_out),
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
        Path(args.standalone_out),
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
