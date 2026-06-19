#!/usr/bin/env python3
"""Assemble a copy-to-SD-root plumOS A30 end-user package."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import tarfile
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

EMPTY_USER_DIRS = [
    "Bios",
    "Imgs",
    "Roms",
    "Saves",
]

EXCLUDED_TOP_LEVELS = {
    "BIOS",
    "Bios",
    "Imgs",
    "Images",
    "Roms",
    "Saves",
    "Screenshots",
    "Videos",
    "bios",
    "roms",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build dist/plumos-sdroot-package as SD-card root contents."
    )
    parser.add_argument(
        "--runtime-dir",
        type=Path,
        default=ROOT / "dist/plumos-runtime-package",
        help="Runtime package directory produced by scripts/build-runtime-package.py.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "dist/plumos-sdroot-package",
    )
    parser.add_argument(
        "--archive",
        type=Path,
        default=None,
        help="Archive path. Defaults to <output-dir>.tar.gz.",
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
        os.symlink(os.readlink(src), dst)
        return
    shutil.copy2(src, dst)


def copy_tree(src: Path, dst: Path) -> tuple[int, int]:
    files = 0
    bytes_total = 0
    for item in iter_files(src):
        rel = item.relative_to(src)
        out = dst / rel
        copy_file(item, out)
        files += 1
        if item.is_file():
            bytes_total += item.stat().st_size
    return files, bytes_total


def ensure_empty_dirs(output_dir: Path) -> None:
    for rel in EMPTY_USER_DIRS:
        (output_dir / rel).mkdir(parents=True, exist_ok=True)
        readme = output_dir / rel / "README.txt"
        readme.write_text(
            "Place your own files here. plumOS release packages do not include ROM, BIOS, save, or media files.\n",
            encoding="utf-8",
        )


def verify_payload(output_dir: Path) -> list[str]:
    required = [
        "miyoo/app/MainUI",
        "plumos/bootstrap/MainUI.wrapper",
        "plumos/bin/plumos-controller-ui-mali",
        "plumos/bin/plumos-env",
        "plumos/config/frontend/systems.json",
        "plumos/ssh/start-ssh.sh",
        "plumos/ssh/stop-ssh.sh",
        "plumos/ssh/bin/dropbear",
        "plumos/ssh/bin/dropbearkey",
        "plumos/ssh/bin/scp",
        "plumos/ssh/etc/authorized_keys",
    ]
    errors: list[str] = []
    for rel in required:
        if not (output_dir / rel).exists():
            errors.append(f"missing SD root payload path: {rel}")
    for rel in EXCLUDED_TOP_LEVELS:
        path = output_dir / rel
        if path.exists():
            files = [p for p in path.rglob("*") if p.is_file()]
            if len(files) > 1 or (files and files[0].name != "README.txt"):
                errors.append(f"user data directory is not empty placeholder-only: {rel}")
    return errors


def write_readme(path: Path) -> None:
    path.write_text(
        """# plumOS A30 SD root package

This archive is intended to be extracted to the root of a freshly formatted A30 SD card.

Expected top-level entries:

- `miyoo/app/MainUI`: plumOS boot wrapper used by the stock boot flow.
- `plumos/`: plumOS runtime.
- `Roms/`, `Bios/`, `Imgs/`, `Saves/`: empty placeholders for user-managed files.

ROMs, BIOS files, save data, states, screenshots, videos, and network secrets are not included.

Rollback note: this fresh-SD package does not include stock `MainUI.stock`. If you install over an existing stock SD card, back up `miyoo/app/MainUI` first.
""",
        encoding="utf-8",
    )


def write_manifest(path: Path, runtime_dir: Path, copied_files: int, copied_bytes: int, archive_name: str) -> None:
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = [
        "plumOS A30 SD root package",
        f"build_time={build_time}",
        f"git_commit={git_value(['rev-parse', 'HEAD'])}",
        f"git_branch={git_value(['rev-parse', '--abbrev-ref', 'HEAD'])}",
        f"git_dirty={'yes' if git_value(['status', '--short'], '') else 'no'}",
        f"archive={archive_name}",
        f"runtime_dir={runtime_dir}",
        "payload_root=sdcard-root",
        f"copied_files={copied_files}",
        f"copied_bytes={copied_bytes}",
        "",
        "[top-level]",
        "miyoo/app/MainUI",
        "plumos/",
        *EMPTY_USER_DIRS,
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
        for child in sorted(package_dir.iterdir()):
            tar.add(child, arcname=child.name, recursive=True)


def main() -> int:
    args = parse_args()
    runtime_dir = args.runtime_dir
    output_dir = args.output_dir
    archive = args.archive or output_dir.with_suffix(".tar.gz")

    runtime_plumos = runtime_dir / "plumos"
    wrapper = runtime_plumos / "bootstrap/MainUI.wrapper"
    if not runtime_plumos.exists():
        raise SystemExit(f"missing runtime plumos directory: {runtime_plumos}")
    if not wrapper.exists():
        raise SystemExit(f"missing runtime MainUI wrapper: {wrapper}")

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    files, bytes_total = copy_tree(runtime_plumos, output_dir / "plumos")
    copy_file(wrapper, output_dir / "miyoo/app/MainUI")
    (output_dir / "miyoo/app/MainUI").chmod(0o755)
    ensure_empty_dirs(output_dir)
    write_readme(output_dir / "README.txt")
    write_manifest(output_dir / "manifest.txt", runtime_dir, files + 1, bytes_total + wrapper.stat().st_size, archive.name)

    errors = verify_payload(output_dir)
    if errors:
        for error in errors:
            print(f"error: {error}", file=os.sys.stderr)
        raise SystemExit(1)

    write_sha256(output_dir)
    if not args.no_archive:
        create_archive(output_dir, archive)

    print(f"package_dir={output_dir}")
    if not args.no_archive:
        print(f"archive={archive}")
        print(f"archive_sha256={sha256_file(archive)}")
    print(f"copied_files={files + 1}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
