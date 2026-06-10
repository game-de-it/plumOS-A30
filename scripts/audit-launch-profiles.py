#!/usr/bin/env python3
"""Audit systems.json launch profiles against staged runtime artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path


RUNTIME_PREFIXES = ("retroarch:", "standalone:")
SKIP_PREFIXES = ("external:", "internal:")


@dataclass
class ProfileUse:
    systems: set[str] = field(default_factory=set)
    defaults: set[str] = field(default_factory=set)


@dataclass
class Artifact:
    profile: str
    paths: list[Path] = field(default_factory=list)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_systems_path(root: Path) -> Path:
    return root / "package/frontend/plumos/config/frontend/systems.json"


def default_artifact_roots(root: Path) -> list[Path]:
    candidates = [
        root / "dist/plumos-libretro-cores",
        root / "dist/plumos-standalone-emulators",
        root / "dist/plumos-retroarch-practical",
    ]
    return [path for path in candidates if path.exists()]


def display_path(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return str(path)


def plumos_root(path: Path) -> Path:
    if (path / "plumos").is_dir():
        return path / "plumos"
    return path


def add_artifact(artifacts: dict[str, Artifact], profile: str, path: Path) -> None:
    artifact = artifacts.setdefault(profile, Artifact(profile=profile))
    artifact.paths.append(path)


def scan_artifacts(roots: list[Path]) -> dict[str, Artifact]:
    artifacts: dict[str, Artifact] = {}
    for root in roots:
        runtime = plumos_root(root)
        cores_dir = runtime / "retroarch/cores"
        if cores_dir.is_dir():
            for core in sorted(cores_dir.glob("*_libretro.so")):
                core_id = core.name.removesuffix("_libretro.so")
                if core_id:
                    add_artifact(artifacts, f"retroarch:{core_id}", core)

        emulators_dir = runtime / "emulators"
        if emulators_dir.is_dir():
            for emulator_dir in sorted(emulators_dir.iterdir()):
                if not emulator_dir.is_dir():
                    continue
                bin_dir = emulator_dir / "bin"
                if not bin_dir.is_dir():
                    continue
                binaries = [p for p in bin_dir.iterdir() if p.is_file()]
                if binaries:
                    add_artifact(artifacts, f"standalone:{emulator_dir.name}", binaries[0])
    return artifacts


def load_profiles(
    systems_path: Path, include_disabled: bool
) -> tuple[dict[str, ProfileUse], list[str]]:
    with systems_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)

    profiles: dict[str, ProfileUse] = {}
    issues: list[str] = []
    for system in data.get("systems", []):
        if not include_disabled and system.get("enabled") is False:
            continue
        system_id = system.get("id", "")
        launch_profiles = list(system.get("launch_profiles", []))
        default_profile = system.get("default_launch_profile", "")
        for profile in launch_profiles:
            profiles.setdefault(profile, ProfileUse()).systems.add(system_id)
        if default_profile:
            use = profiles.setdefault(default_profile, ProfileUse())
            use.systems.add(system_id)
            use.defaults.add(system_id)
            if default_profile not in launch_profiles:
                issues.append(
                    f"default_not_listed system={system_id} profile={default_profile}"
                )
    return profiles, issues


def expected_runtime_path(profile: str) -> str:
    if ":" not in profile:
        return "-"
    kind, value = profile.split(":", 1)
    if kind == "retroarch":
        return f"plumos/retroarch/cores/{value}_libretro.so"
    if kind == "standalone":
        return f"plumos/emulators/{value}/bin/*"
    return "-"


def profile_selected(
    profile: str, use: ProfileUse | None, profile_filter: set[str], system_filter: set[str]
) -> bool:
    if profile_filter and profile not in profile_filter:
        return False
    if system_filter:
        if use is None:
            return False
        if not (use.systems & system_filter):
            return False
    return True


def runtime_selected(profile: str, runtime: str) -> bool:
    if runtime == "all":
        return True
    return profile.startswith(f"{runtime}:")


def format_systems(use: ProfileUse) -> str:
    systems = sorted(use.systems)
    defaults = sorted(use.defaults)
    if not defaults:
        return ",".join(systems)
    marked = [f"{system}*" if system in use.defaults else system for system in systems]
    return ",".join(marked)


def print_profiles(
    title: str,
    profiles: list[tuple[str, ProfileUse | None, Artifact | None]],
    repo: Path,
    verbose_paths: bool,
) -> None:
    if not profiles:
        return
    print(title)
    for profile, use, artifact in profiles:
        systems = format_systems(use) if use else "-"
        if artifact and artifact.paths:
            paths = ", ".join(display_path(path, repo) for path in artifact.paths[:3])
            if len(artifact.paths) > 3:
                paths += f", +{len(artifact.paths) - 3}"
        else:
            paths = expected_runtime_path(profile)
        print(f"  {profile:<34} systems={systems:<28} path={paths}")
        if verbose_paths and artifact:
            for path in artifact.paths:
                print(f"    - {display_path(path, repo)}")
    print()


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description="Compare systems.json launch_profiles with staged runtime artifacts."
    )
    parser.add_argument(
        "--systems",
        default=str(default_systems_path(root)),
        help="systems.json path. Default: package/frontend/plumos/config/frontend/systems.json",
    )
    parser.add_argument(
        "--root",
        action="append",
        default=[],
        help=(
            "Runtime artifact root to scan. May point to a dist package, SD card root, "
            "or plumOS root. Repeatable. Defaults to known dist packages."
        ),
    )
    parser.add_argument(
        "--profile",
        action="append",
        default=[],
        help="Limit output/checks to one launch profile, for example retroarch:quicknes.",
    )
    parser.add_argument(
        "--system",
        action="append",
        default=[],
        help="Limit registered profile output/checks to one system id.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when a registered runtime-backed profile has no artifact.",
    )
    parser.add_argument(
        "--runtime",
        choices=("all", "retroarch", "standalone"),
        default="all",
        help="Limit runtime-backed checks to one profile prefix. Default: all.",
    )
    parser.add_argument(
        "--include-disabled",
        action="store_true",
        help="Include systems where enabled is false. Disabled systems are skipped by default.",
    )
    parser.add_argument(
        "--fail-on-extra",
        action="store_true",
        help="Exit non-zero when an artifact exists but no systems.json profile uses it.",
    )
    parser.add_argument("--show-ok", action="store_true", help="Show matched profiles.")
    parser.add_argument("--verbose-paths", action="store_true", help="Show every artifact path.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    systems_path = Path(args.systems)
    artifact_roots = [Path(path) for path in args.root] or default_artifact_roots(root)
    profile_filter = set(args.profile)
    system_filter = set(args.system)

    profiles, source_issues = load_profiles(systems_path, args.include_disabled)
    artifacts = scan_artifacts(artifact_roots)

    missing: list[tuple[str, ProfileUse | None, Artifact | None]] = []
    skipped: list[tuple[str, ProfileUse | None, Artifact | None]] = []
    ok: list[tuple[str, ProfileUse | None, Artifact | None]] = []

    for profile, use in sorted(profiles.items()):
        if not profile_selected(profile, use, profile_filter, system_filter):
            continue
        if profile.startswith(RUNTIME_PREFIXES):
            if not runtime_selected(profile, args.runtime):
                continue
            artifact = artifacts.get(profile)
            if artifact:
                ok.append((profile, use, artifact))
            else:
                missing.append((profile, use, None))
        elif profile.startswith(SKIP_PREFIXES):
            if args.runtime != "all":
                continue
            skipped.append((profile, use, None))
        else:
            missing.append((profile, use, None))

    extras: list[tuple[str, ProfileUse | None, Artifact | None]] = []
    for profile, artifact in sorted(artifacts.items()):
        if profile in profiles:
            continue
        if not runtime_selected(profile, args.runtime):
            continue
        if not profile_selected(profile, None, profile_filter, system_filter):
            continue
        extras.append((profile, None, artifact))

    selected_source_issues = []
    for issue in source_issues:
        issue_profile = issue.rsplit("profile=", 1)[-1]
        issue_system = issue.split("system=", 1)[-1].split(" ", 1)[0]
        if profile_filter and issue_profile not in profile_filter:
            continue
        if system_filter and issue_system not in system_filter:
            continue
        selected_source_issues.append(issue)

    print("Launch profile audit")
    print(f"systems: {display_path(systems_path, root)}")
    print("roots:")
    if artifact_roots:
        for artifact_root in artifact_roots:
            print(f"  - {display_path(artifact_root, root)}")
    else:
        print("  - (none)")
    print()
    print(
        "summary: "
        f"ok={len(ok)} missing={len(missing)} extras={len(extras)} "
        f"skipped={len(skipped)} source_issues={len(selected_source_issues)}"
    )
    print()

    if selected_source_issues:
        print("Source issues")
        for issue in selected_source_issues:
            print(f"  {issue}")
        print()

    if args.show_ok:
        print_profiles("OK registered runtime profiles", ok, root, args.verbose_paths)
    print_profiles("Missing registered runtime profiles", missing, root, args.verbose_paths)
    print_profiles("Available artifacts not listed in systems.json", extras, root, args.verbose_paths)
    print_profiles("Skipped non-runtime profiles", skipped, root, args.verbose_paths)

    failed = False
    if args.strict and (missing or selected_source_issues):
        failed = True
    if args.fail_on_extra and extras:
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
