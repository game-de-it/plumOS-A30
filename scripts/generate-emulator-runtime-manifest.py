#!/usr/bin/env python3
"""Generate the FE emulator/runtime adoption manifest."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

FIELDS = [
    "system_id",
    "display_name",
    "runtime",
    "launch_profile",
    "component_id",
    "default_profile",
    "verification_status",
    "adoption_state",
    "frontend_runtime",
    "frontend_source",
    "frontend_ref",
    "source_repo",
    "source_ref",
    "version_policy",
    "build_recipe",
    "build_options",
    "build_log",
    "notes",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Join FE verification targets, libretro recipes, and runtime build "
            "defaults into docs/emulator-runtime-manifest.tsv."
        )
    )
    parser.add_argument(
        "--libretro-targets",
        default=ROOT / "docs/emulator-fe-libretro-targets.tsv",
        type=Path,
    )
    parser.add_argument(
        "--standalone-targets",
        default=ROOT / "docs/emulator-fe-standalone-targets.tsv",
        type=Path,
    )
    parser.add_argument(
        "--built-cores",
        default=ROOT / "docs/libretro-built-cores.tsv",
        type=Path,
    )
    parser.add_argument(
        "--version-inventory",
        default=ROOT / "docs/libretro-core-version-inventory.tsv",
        type=Path,
    )
    parser.add_argument(
        "--output",
        default=None,
        type=Path,
        help="Output path. Defaults to stdout.",
    )
    return parser.parse_args()


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def clean_cell(value: str) -> str:
    return " ".join(str(value).replace("\t", " ").splitlines())


def read_shell_defaults(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    assignment = re.compile(r"^([A-Z0-9_]+)=(.*)$")
    default_expr = re.compile(r"^\$\{([A-Z0-9_]+):-(.*)\}$")
    var_expr = re.compile(r"\$\{([A-Z0-9_]+)\}")

    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        match = assignment.match(line)
        if not match:
            continue
        key, rhs = match.groups()
        rhs = rhs.strip()
        if (
            (rhs.startswith('"') and rhs.endswith('"'))
            or (rhs.startswith("'") and rhs.endswith("'"))
        ):
            rhs = rhs[1:-1]
        expr = default_expr.match(rhs)
        if not expr:
            continue
        default = expr.group(2)
        if (
            (default.startswith('"') and default.endswith('"'))
            or (default.startswith("'") and default.endswith("'"))
        ):
            default = default[1:-1]

        def replace_var(var_match: re.Match[str]) -> str:
            return values.get(var_match.group(1), var_match.group(0))

        previous = None
        while previous != default:
            previous = default
            default = var_expr.sub(replace_var, default)
        values[key] = default
    return values


def runtime_defaults() -> dict[str, dict[str, str]]:
    retro = read_shell_defaults(
        ROOT / "docker/plumos-toolchain/scripts/build-retroarch-practical.sh"
    )
    pico = read_shell_defaults(ROOT / "docker/plumos-toolchain/scripts/build-picoarch.sh")
    standalone = read_shell_defaults(
        ROOT / "docker/plumos-toolchain/scripts/build-standalone-emulators.sh"
    )
    pyxel = read_shell_defaults(ROOT / "docker/plumos-toolchain/scripts/build-pyxel-a30.sh")

    return {
        "RA": {
            "frontend_runtime": "retroarch-practical",
            "frontend_source": "https://github.com/libretro/RetroArch",
            "frontend_ref": retro.get("RETROARCH_TAG", retro.get("RETROARCH_VERSION", "")),
        },
        "PICO": {
            "frontend_runtime": "picoarch",
            "frontend_source": pico.get("PICOARCH_REPO", ""),
            "frontend_ref": pico.get("PICOARCH_REF", ""),
        },
        "ppsspp": {
            "source_repo": standalone.get("PPSSPP_REPO", ""),
            "source_ref": standalone.get("PPSSPP_REF", ""),
            "build_options": "PPSSPP_PATCH_MODE=a30; logical UI; A30 gamepad",
        },
        "scummvm": {
            "source_repo": standalone.get("SCUMMVM_REPO", ""),
            "source_ref": standalone.get("SCUMMVM_REF", ""),
            "build_options": "engines=" + standalone.get("SCUMMVM_ENGINES", ""),
        },
        "easyrpg": {
            "source_repo": standalone.get("EASYRPG_REPO", ""),
            "source_ref": standalone.get("EASYRPG_REF", ""),
            "build_options": "SDL2/A30 rotation; mpg123/Vorbis/Opus/MOD/LZH/Freetype",
        },
        "pcsx_rearmed": {
            "source_repo": standalone.get("PCSX_REARMED_REPO", ""),
            "source_ref": standalone.get("PCSX_REARMED_REF", ""),
            "build_options": "generic SDL1 frontend; native fb32 A30 rotation/menu patches",
        },
        "openbor": {
            "source_repo": standalone.get("OPENBOR_REPO", ""),
            "source_ref": standalone.get("OPENBOR_REF", ""),
            "build_options": "SDL2 input/audio; A30 direct Mali presenter; WebM disabled",
        },
        "pyxel": {
            "source_repo": pyxel.get("PYXEL_REPO", ""),
            "source_ref": pyxel.get("PYXEL_REF", ""),
            "build_options": (
                "maturin --features sdl2_dynamic; target="
                + pyxel.get("TARGET_TRIPLE", "")
                + "; rust="
                + pyxel.get("RUST_TOOLCHAIN", "")
                + "; SDL a30mali shim"
            ),
        },
    }


def adoption_state(status: str) -> str:
    if status == "pass":
        return "adopted"
    if status == "pass_init":
        return "candidate_init"
    if status == "untested":
        return "candidate_waiting_content"
    return "excluded"


def libretro_rows(
    targets: list[dict[str, str]],
    built: dict[str, dict[str, str]],
    versions: dict[str, dict[str, str]],
    defaults: dict[str, dict[str, str]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for target in targets:
        status = target.get("verification_status", "")
        if target.get("fe_executable") != "yes" or status == "retired":
            continue
        core_id = target.get("core_id", "")
        runtime = target.get("runtime", "")
        core = built.get(core_id, {})
        version = versions.get(core_id, {})
        frontend = defaults.get(runtime, {})
        notes = target.get("notes", "")
        if runtime == "PICO" and core_id == "fceumm":
            notes = (notes + "; " if notes else "") + (
                "launcher uses PicoArch bundled fceumm compatibility core when present"
            )
        rows.append(
            {
                "system_id": target.get("system_id", ""),
                "display_name": target.get("display_name", ""),
                "runtime": runtime,
                "launch_profile": target.get("launch_profile", ""),
                "component_id": core_id,
                "default_profile": target.get("default_profile", ""),
                "verification_status": status,
                "adoption_state": adoption_state(status),
                "frontend_runtime": frontend.get("frontend_runtime", ""),
                "frontend_source": frontend.get("frontend_source", ""),
                "frontend_ref": frontend.get("frontend_ref", ""),
                "source_repo": version.get("plumos_repo", ""),
                "source_ref": core.get("commit") or version.get("plumos_ref", ""),
                "version_policy": version.get("version_policy", ""),
                "build_recipe": "docker/plumos-toolchain/libretro-core-recipes.tsv",
                "build_options": " ".join(
                    part
                    for part in [
                        core.get("make_args", ""),
                        f"jobs={core.get('make_jobs', '')}" if core.get("make_jobs") else "",
                    ]
                    if part
                ),
                "build_log": core.get("log", ""),
                "notes": notes,
            }
        )
    return rows


def standalone_rows(
    targets: list[dict[str, str]],
    defaults: dict[str, dict[str, str]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for target in targets:
        status = target.get("verification_status", "")
        if target.get("fe_executable") != "yes" or status == "retired":
            continue
        emulator_id = target.get("emulator_id", "")
        if target.get("runtime") == "PYXEL":
            source = defaults.get("pyxel", {})
            recipe = "docker/plumos-toolchain/scripts/build-pyxel-a30.sh"
            frontend_runtime = "pyxel-a30"
            frontend_source = source.get("source_repo", "")
            frontend_ref = source.get("source_ref", "")
            build_log = "dist/plumos-pyxel-a30/plumos/share/doc/plumos-pyxel-a30/manifest.txt"
        else:
            source = defaults.get(emulator_id, {})
            recipe = "docker/plumos-toolchain/scripts/build-standalone-emulators.sh"
            frontend_runtime = "standalone"
            frontend_source = source.get("source_repo", "")
            frontend_ref = source.get("source_ref", "")
            build_log = f"docs/build-logs/{emulator_id}.log" if emulator_id else ""
        rows.append(
            {
                "system_id": target.get("system_id", ""),
                "display_name": target.get("display_name", ""),
                "runtime": target.get("runtime", ""),
                "launch_profile": target.get("launch_profile", ""),
                "component_id": emulator_id,
                "default_profile": target.get("default_profile", ""),
                "verification_status": status,
                "adoption_state": adoption_state(status),
                "frontend_runtime": frontend_runtime,
                "frontend_source": frontend_source,
                "frontend_ref": frontend_ref,
                "source_repo": source.get("source_repo", ""),
                "source_ref": source.get("source_ref", ""),
                "version_policy": "standalone_pinned",
                "build_recipe": recipe,
                "build_options": source.get("build_options", ""),
                "build_log": build_log,
                "notes": target.get("notes", ""),
            }
        )
    return rows


def write_tsv(path: Path | None, rows: list[dict[str, str]]) -> None:
    import sys

    handle = path.open("w", encoding="utf-8", newline="") if path else sys.stdout
    close = path is not None
    try:
        writer = csv.DictWriter(
            handle,
            fieldnames=FIELDS,
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()
        for row in rows:
            writer.writerow({field: clean_cell(row.get(field, "")) for field in FIELDS})
    finally:
        if close:
            handle.close()


def main() -> int:
    args = parse_args()
    built = {row["core_id"]: row for row in read_tsv(args.built_cores)}
    versions = {row["id"]: row for row in read_tsv(args.version_inventory)}
    defaults = runtime_defaults()
    rows = libretro_rows(read_tsv(args.libretro_targets), built, versions, defaults)
    rows.extend(standalone_rows(read_tsv(args.standalone_targets), defaults))
    rows.sort(
        key=lambda row: (
            row["system_id"].lower(),
            row["runtime"],
            row["launch_profile"],
        )
    )
    write_tsv(args.output, rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
