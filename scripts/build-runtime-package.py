#!/usr/bin/env python3
"""Assemble a deployable plumOS A30 runtime package from dist artifacts."""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import shutil
import subprocess
import tarfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

MUTABLE_PATHS = [
    "plumos/config/frontend/settings.json",
    "plumos/config/system/settings.json",
    "plumos/config/network/settings.json",
    "plumos/config/performance/settings.json",
    "plumos/config/standalone",
    "plumos/state",
    "plumos/logs",
    "plumos/retroarch/config/retroarch-minimal.cfg",
    "plumos/retroarch/config/retroarch-practical.cfg",
    "plumos/retroarch/saves",
    "plumos/retroarch/states",
    "plumos/retroarch/playlists",
]

REQUIRED_RUNTIME_PATHS = {
    "RA": ["plumos/bin/plumos-retroarch-launch", "plumos/retroarch/bin/retroarch"],
    "PICO": ["plumos/bin/plumos-picoarch-launch", "plumos/emulators/picoarch/bin/picoarch"],
    "SA": ["plumos/bin/plumos-standalone-launch"],
    "PYXEL": ["plumos/bin/plumos-pyxel-a30-launch", "plumos/bin/plumos-pyxel-a30"],
}

STANDALONE_BINARY_PATHS = {
    "easyrpg": "plumos/emulators/easyrpg/bin/easyrpg-player",
    "openbor": "plumos/emulators/openbor/bin/OpenBOR",
    "pcsx_rearmed": "plumos/emulators/pcsx_rearmed/bin/pcsx",
    "ppsspp": "plumos/emulators/ppsspp/bin/PPSSPPSDL",
    "scummvm": "plumos/emulators/scummvm/bin/scummvm",
}

BASE_RUNTIME_PATHS = [
    "plumos/ssh/start-ssh.sh",
    "plumos/ssh/stop-ssh.sh",
    "plumos/ssh/bin/dropbear",
    "plumos/ssh/bin/dropbearkey",
    "plumos/ssh/bin/scp",
    "plumos/ssh/etc/authorized_keys",
]

PPSSPP_FACTORY_STATE_PATHS = [
    "plumos/state/standalone/ppsspp/config/plumos-a30-ppsspp-layout.ini",
    "plumos/state/standalone/ppsspp/config/ppsspp/PSP/SYSTEM/ppsspp.ini",
    "plumos/state/standalone/ppsspp/config/ppsspp/PSP/SYSTEM/controls.ini",
    "plumos/state/standalone/ppsspp/.config/ppsspp/PSP/SYSTEM/ppsspp.ini",
    "plumos/state/standalone/ppsspp/.config/ppsspp/PSP/SYSTEM/controls.ini",
]


@dataclass(frozen=True)
class Component:
    name: str
    path: Path
    required: bool = True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build dist/plumos-runtime-package from selected dist artifacts."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "dist/plumos-runtime-package",
    )
    parser.add_argument(
        "--archive",
        type=Path,
        default=None,
        help="Archive path. Defaults to <output-dir>.tar.gz.",
    )
    parser.add_argument(
        "--standalone-dir",
        type=Path,
        default=None,
        help=(
            "Standalone emulator dist. Defaults to "
            "dist/plumos-standalone-emulators-adopted if present, otherwise "
            "dist/plumos-standalone-emulators."
        ),
    )
    parser.add_argument(
        "--runtime-manifest",
        type=Path,
        default=ROOT / "docs/emulator-runtime-manifest.tsv",
    )
    parser.add_argument("--no-archive", action="store_true")
    return parser.parse_args()


def git_value(args: list[str], fallback: str = "unknown") -> str:
    result = subprocess.run(
        ["git", "-C", str(ROOT), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    value = result.stdout.strip()
    return value if value else fallback


def default_standalone_dir() -> Path:
    adopted = ROOT / "dist/plumos-standalone-emulators-adopted"
    if adopted.exists():
        return adopted
    return ROOT / "dist/plumos-standalone-emulators"


def components(standalone_dir: Path) -> list[Component]:
    dist = ROOT / "dist"
    return [
        Component("userland", dist / "plumos-userland", required=False),
        Component("bootstrap", dist / "plumos-bootstrap"),
        Component("frontend", dist / "plumos-frontend"),
        Component("runtime-probe", dist / "plumos-runtime-probe"),
        Component("joystickd", dist / "plumos-joystickd"),
        Component("network-services", dist / "plumos-network-services"),
        Component("ssh-kit", dist / "plumos-a30-ssh-kit"),
        Component("retroarch-practical", dist / "plumos-retroarch-practical"),
        Component("sdl2-runtime", dist / "plumos-sdl2-runtime"),
        Component("python-runtime", dist / "plumos-python-runtime"),
        Component("pyxel-a30", dist / "plumos-pyxel-a30"),
        Component("nextcommander", dist / "plumos-nextcommander"),
        Component("gmu", dist / "plumos-gmu"),
        Component("libretro-cores", dist / "plumos-libretro-cores-onion-lock-all"),
        Component("libretro-cores-mgba-fix", dist / "plumos-libretro-cores-mgba-fix", required=False),
        Component("picoarch", dist / "plumos-picoarch"),
        Component("standalone-emulators", standalone_dir),
    ]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def iter_files(path: Path) -> list[Path]:
    return sorted(p for p in path.rglob("*") if p.is_file() or p.is_symlink())


def copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() or dst.is_symlink():
        if dst.is_dir() and not dst.is_symlink():
            shutil.rmtree(dst)
        else:
            dst.unlink()
    if src.is_symlink():
        target = os.readlink(src)
        os.symlink(target, dst)
        return
    shutil.copy2(src, dst)


def copy_tree_overlay(src: Path, dst: Path, component_name: str) -> tuple[int, int, list[str]]:
    copied = 0
    bytes_total = 0
    collisions: list[str] = []
    for item in iter_files(src):
        if item.name == ".DS_Store":
            continue
        rel = item.relative_to(src)
        out = dst / rel
        if out.exists() or out.is_symlink():
            if item.is_file() and out.is_file():
                try:
                    same = sha256_file(item) == sha256_file(out)
                except OSError:
                    same = False
            else:
                same = item.is_symlink() and out.is_symlink() and os.readlink(item) == os.readlink(out)
            if not same:
                collisions.append(f"{component_name}:{rel}")
        copy_file(item, out)
        copied += 1
        if item.is_file():
            bytes_total += item.stat().st_size
    return copied, bytes_total, collisions


def read_runtime_manifest(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def verify_runtime_payload(package_dir: Path, manifest_path: Path) -> list[str]:
    errors: list[str] = []
    rows = read_runtime_manifest(manifest_path)
    for rel in BASE_RUNTIME_PATHS:
        if not (package_dir / rel).exists():
            errors.append(f"missing base runtime path: {rel}")
    for rel in PPSSPP_FACTORY_STATE_PATHS:
        if not (package_dir / rel).exists():
            errors.append(f"missing PPSSPP factory state path: {rel}")

    for runtime in sorted({row["runtime"] for row in rows}):
        for rel in REQUIRED_RUNTIME_PATHS.get(runtime, []):
            if not (package_dir / rel).exists():
                errors.append(f"missing runtime path for {runtime}: {rel}")

    for row in rows:
        runtime = row["runtime"]
        component_id = row["component_id"]
        if runtime in {"RA", "PICO"}:
            rel = f"plumos/retroarch/cores/{component_id}_libretro.so"
        elif runtime == "SA":
            rel = STANDALONE_BINARY_PATHS.get(component_id, "")
        elif runtime == "PYXEL":
            rel = "plumos/bin/plumos-pyxel-a30-launch"
        else:
            rel = ""
        if rel and not (package_dir / rel).exists():
            errors.append(f"missing {runtime} {row['launch_profile']}: {rel}")
    return errors


def write_install_script(path: Path) -> None:
    mutable = "\n".join(MUTABLE_PATHS)
    path.write_text(
        f"""#!/bin/sh
set -eu

PACKAGE_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TARGET_ROOT="${{1:-/mnt/SDCARD}}"
PAYLOAD_DIR="${{PACKAGE_DIR}}"
BACKUP_ROOT="${{PLUMOS_RUNTIME_BACKUP_ROOT:-${{TARGET_ROOT}}/plumos-backups}}"
BACKUP_ENABLED="${{PLUMOS_RUNTIME_BACKUP:-1}}"
PRESERVE_DIR="${{TMPDIR:-/tmp}}/plumos-runtime-preserve-$$"

usage() {{
  cat <<'USAGE'
Usage: ./install-plumos-runtime.sh [SDCARD_ROOT]

Default SDCARD_ROOT is /mnt/SDCARD.
Set PLUMOS_RUNTIME_BACKUP=0 to skip the pre-install backup.
USAGE
}}

case "${{1:-}}" in
  -h|--help) usage; exit 0 ;;
esac

[ -d "${{PAYLOAD_DIR}}/plumos" ] || {{
  echo "error: payload plumos/ directory not found next to installer" >&2
  exit 1
}}

mkdir -p "${{TARGET_ROOT}}" "${{PRESERVE_DIR}}"

if [ "${{BACKUP_ENABLED}}" != "0" ] && [ -d "${{TARGET_ROOT}}/plumos" ]; then
  stamp="$(date +%Y%m%d-%H%M%S 2>/dev/null || echo runtime)"
  mkdir -p "${{BACKUP_ROOT}}"
  backup="${{BACKUP_ROOT}}/plumos-runtime-${{stamp}}.tar.gz"
  tar -C "${{TARGET_ROOT}}" -czf "${{backup}}" plumos
  echo "backup=${{backup}}"
fi

: > "${{PRESERVE_DIR}}/manifest"
preserve_path() {{
  rel=$1
  src="${{TARGET_ROOT}}/${{rel}}"
  dst="${{PRESERVE_DIR}}/${{rel}}"
  [ -e "${{src}}" ] || return 0
  mkdir -p "$(dirname "${{dst}}")"
  if [ -d "${{src}}" ] && [ ! -L "${{src}}" ]; then
    cp -Rp "${{src}}" "${{dst}}"
  else
    cp -Pp "${{src}}" "${{dst}}"
  fi
  printf '%s\\n' "${{rel}}" >> "${{PRESERVE_DIR}}/manifest"
}}

while IFS= read -r rel; do
  [ -n "${{rel}}" ] || continue
  preserve_path "${{rel}}"
done <<'MUTABLE'
{mutable}
MUTABLE

tar -C "${{PAYLOAD_DIR}}" -cf - plumos | tar -C "${{TARGET_ROOT}}" -xf -

while IFS= read -r rel; do
  [ -n "${{rel}}" ] || continue
  src="${{PRESERVE_DIR}}/${{rel}}"
  dst="${{TARGET_ROOT}}/${{rel}}"
  [ -e "${{src}}" ] || continue
  mkdir -p "$(dirname "${{dst}}")"
  if [ -d "${{src}}" ] && [ ! -L "${{src}}" ]; then
    mkdir -p "${{dst}}"
    cp -Rp "${{src}}"/. "${{dst}}"/
  else
    cp -Pp "${{src}}" "${{dst}}"
  fi
  echo "preserved=${{rel}}"
done < "${{PRESERVE_DIR}}/manifest"

rm -rf "${{PRESERVE_DIR}}"
echo "installed=${{TARGET_ROOT}}/plumos"
""",
        encoding="utf-8",
    )
    path.chmod(0o755)


def write_package_docs(package_dir: Path, component_rows: list[dict[str, str]], archive_name: str) -> None:
    doc_dir = package_dir / "plumos/share/doc/plumos-runtime-package"
    doc_dir.mkdir(parents=True, exist_ok=True)
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    git_commit = git_value(["rev-parse", "HEAD"])
    git_dirty = git_value(["status", "--short"], "")
    manifest_lines = [
        "plumOS A30 runtime package",
        f"build_time={build_time}",
        f"git_commit={git_commit}",
        f"git_dirty={'yes' if git_dirty else 'no'}",
        f"archive={archive_name}",
        "payload_root=plumos",
        "install_script=install-plumos-runtime.sh",
        "",
        "[components]",
    ]
    for row in component_rows:
        manifest_lines.append(
            "{name}\tpath={path}\trequired={required}\tfiles={files}\tbytes={bytes}\tstatus={status}".format(
                **row
            )
        )
    manifest_lines.extend(
        [
            "",
            "[mutable-preserve-paths]",
            *MUTABLE_PATHS,
        ]
    )
    (doc_dir / "manifest.txt").write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")

    shutil.copy2(ROOT / "docs/emulator-runtime-manifest.tsv", doc_dir / "emulator-runtime-manifest.tsv")
    shutil.copy2(ROOT / "docs/emulator-runtime-manifest.md", doc_dir / "emulator-runtime-manifest.md")
    shutil.copy2(ROOT / "docs/runtime-package.md", doc_dir / "runtime-package.md")


def write_sha256(package_dir: Path) -> None:
    lines: list[str] = []
    for item in iter_files(package_dir):
        if item.name == "sha256sum.txt":
            continue
        if item.is_symlink():
            continue
        rel = item.relative_to(package_dir)
        lines.append(f"{sha256_file(item)}  {rel}")
    (package_dir / "sha256sum.txt").write_text("\n".join(sorted(lines)) + "\n", encoding="utf-8")


def create_archive(package_dir: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    if archive.exists():
        archive.unlink()
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(package_dir, arcname=package_dir.name, recursive=True)


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir
    archive = args.archive or output_dir.with_suffix(".tar.gz")
    standalone_dir = args.standalone_dir or default_standalone_dir()
    component_rows: list[dict[str, str]] = []
    overlay_rows: list[str] = []

    if not args.runtime_manifest.exists():
        raise SystemExit(f"missing runtime manifest: {args.runtime_manifest}")

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    for component in components(standalone_dir):
        src = component.path
        src_payload = src / "plumos"
        status = "included"
        files = 0
        bytes_total = 0
        if not src_payload.exists():
            if component.required:
                raise SystemExit(f"missing required component payload: {src_payload}")
            status = "missing_optional"
        else:
            files, bytes_total, overrides = copy_tree_overlay(
                src_payload, output_dir / "plumos", component.name
            )
            overlay_rows.extend(overrides)
        component_rows.append(
            {
                "name": component.name,
                "path": str(src.relative_to(ROOT) if src.is_relative_to(ROOT) else src),
                "required": "yes" if component.required else "no",
                "files": str(files),
                "bytes": str(bytes_total),
                "status": status,
            }
        )

    package_errors = verify_runtime_payload(output_dir, args.runtime_manifest)
    if package_errors:
        for error in package_errors:
            print(f"error: {error}", file=os.sys.stderr)
        raise SystemExit(1)

    write_install_script(output_dir / "install-plumos-runtime.sh")
    write_package_docs(output_dir, component_rows, archive.name)
    if overlay_rows:
        overlay_path = output_dir / "plumos/share/doc/plumos-runtime-package/overlay-overrides.txt"
        overlay_path.write_text("\n".join(overlay_rows) + "\n", encoding="utf-8")
    write_sha256(output_dir)
    if not args.no_archive:
        create_archive(output_dir, archive)

    print(f"package_dir={output_dir}")
    if not args.no_archive:
        print(f"archive={archive}")
        print(f"archive_sha256={sha256_file(archive)}")
    print(f"components={len(component_rows)}")
    print(f"overlay_overrides={len(overlay_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
