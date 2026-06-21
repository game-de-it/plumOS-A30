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
DEFAULT_STOCK_SDCARD_DIR = ROOT / "artifacts/stock-sdl-probe/extracted/mnt/SDCARD"

EMPTY_USER_DIRS = [
    "Bios",
    "Images",
    "Imgs",
    "Roms",
    "Saves",
]

USER_DIR_READMES = {
    "Bios": "Place your own BIOS files here. plumOS release packages do not include BIOS files.\n",
    "Images": (
        "Place plumOS ROM thumbnails here, under Images/<ROM directory name>/.\n"
        "Example: Roms/snes/Game.sfc uses Images/snes/Game.png\n"
        "plumOS release packages do not include user media files.\n"
    ),
    "Imgs": (
        "Legacy StockOS artwork may live here on existing cards.\n"
        "For new plumOS thumbnails, use Images/<ROM directory name>/ instead.\n"
    ),
    "Roms": "Place your own ROM files here. plumOS release packages do not include ROM files.\n",
    "Saves": "User save data and states may live here. plumOS release packages do not include saves.\n",
}

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

STOCK_PAYLOAD_EXCLUDED_TOP_LEVELS = EXCLUDED_TOP_LEVELS | {
    ".config",
    ".Spotlight-V100",
    ".TemporaryItems",
    ".Trashes",
    ".fseventsd",
    "System Volume Information",
    "plumos",
}

STOCK_PAYLOAD_REQUIRED = [
    "miyoo/app/MainUI.stock",
    "miyoo/app/keymon",
    "miyoo/app/sdlloading",
    "miyoo/lib/libSDL-1.2.so.0",
    "miyoo/lib/libSDL2-2.0.so.0",
    "miyoo/lib/libshmvar.so",
    "miyoo/lib/libtmenu.so",
]

PPSSPP_FACTORY_STATE_PATHS = [
    "plumos/state/standalone/ppsspp/config/plumos-a30-ppsspp-layout.ini",
    "plumos/state/standalone/ppsspp/config/ppsspp/PSP/SYSTEM/ppsspp.ini",
    "plumos/state/standalone/ppsspp/config/ppsspp/PSP/SYSTEM/controls.ini",
    "plumos/state/standalone/ppsspp/.config/ppsspp/PSP/SYSTEM/ppsspp.ini",
    "plumos/state/standalone/ppsspp/.config/ppsspp/PSP/SYSTEM/controls.ini",
]

NOTICE_FILES = [
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES.ja.md",
]


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
        "--stock-sdcard-dir",
        type=Path,
        default=DEFAULT_STOCK_SDCARD_DIR,
        help=(
            "Stock SD-card payload directory copied before the plumOS overlay. "
            "ROM/BIOS/save/media/user-data top-level directories are excluded."
        ),
    )
    parser.add_argument(
        "--archive",
        type=Path,
        default=None,
        help="Archive path. Defaults to <output-dir>.tar.gz.",
    )
    parser.add_argument(
        "--no-stock-payload",
        action="store_true",
        help="Developer-only escape hatch; official SD-root packages include stock payload files.",
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


def should_skip_stock_payload(rel: Path) -> bool:
    if not rel.parts:
        return True
    if rel.parts[0] in STOCK_PAYLOAD_EXCLUDED_TOP_LEVELS:
        return True
    for part in rel.parts:
        if part in {".DS_Store", "Thumbs.db"}:
            return True
        if part.startswith("._"):
            return True
    return False


def copy_stock_payload(src: Path, dst: Path) -> tuple[int, int]:
    if not src.exists():
        raise SystemExit(
            f"missing stock SD-card payload: {src}\n"
            "Provide --stock-sdcard-dir pointing at a stock /mnt/SDCARD snapshot, "
            "or use --no-stock-payload only for local developer experiments."
        )

    files = 0
    bytes_total = 0
    for item in iter_files(src):
        rel = item.relative_to(src)
        if should_skip_stock_payload(rel):
            continue
        out = dst / rel
        copy_file(item, out)
        files += 1
        if item.is_file():
            bytes_total += item.stat().st_size
    return files, bytes_total


def preserve_stock_mainui(output_dir: Path) -> tuple[int, int]:
    mainui = output_dir / "miyoo/app/MainUI"
    stock_mainui = output_dir / "miyoo/app/MainUI.stock"
    if mainui.exists() and not stock_mainui.exists():
        copy_file(mainui, stock_mainui)
        return 1, stock_mainui.stat().st_size
    return 0, 0


def ensure_empty_dirs(output_dir: Path) -> None:
    for rel in EMPTY_USER_DIRS:
        (output_dir / rel).mkdir(parents=True, exist_ok=True)
        readme = output_dir / rel / "README.txt"
        readme.write_text(USER_DIR_READMES[rel], encoding="utf-8")


def copy_notice_files(output_dir: Path) -> None:
    for rel in NOTICE_FILES:
        src = ROOT / rel
        if src.exists():
            copy_file(src, output_dir / rel)


def verify_payload(output_dir: Path, require_stock_payload: bool) -> list[str]:
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
        "plumos/ssh/etc/password.hash",
        *PPSSPP_FACTORY_STATE_PATHS,
    ]
    if require_stock_payload:
        required.extend(STOCK_PAYLOAD_REQUIRED)
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

- `miyoo/`: stock SD-card runtime files plus the plumOS boot wrapper at
  `miyoo/app/MainUI`.
- `plumos/`: plumOS runtime.
- `LICENSE`, `THIRD_PARTY_NOTICES.md`: plumOS license and bundled component
  attribution.
- Optional stock top-level runtimes such as `RetroArch/` or `Emu/`, when present
  in the stock SD-card payload input.
- `Roms/`, `Bios/`, `Images/`, `Imgs/`, `Saves/`: empty placeholders for user-managed files.

ROMs, BIOS files, save data, states, screenshots, videos, and network secrets are not included.
The default SSH password is `plumos`. Optional public keys may be placed in
`plumos/ssh/etc/authorized_keys`.

Rollback note: the stock `miyoo/app/MainUI` is preserved as
`miyoo/app/MainUI.stock` when it is available in the stock payload. plumOS
overlays `miyoo/app/MainUI` with its boot wrapper.
""",
        encoding="utf-8",
    )


def write_manifest(
    path: Path,
    runtime_dir: Path,
    stock_sdcard_dir: Path | None,
    stock_files: int,
    stock_bytes: int,
    copied_files: int,
    copied_bytes: int,
    archive_name: str,
) -> None:
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = [
        "plumOS A30 SD root package",
        f"build_time={build_time}",
        f"git_commit={git_value(['rev-parse', 'HEAD'])}",
        f"git_branch={git_value(['rev-parse', '--abbrev-ref', 'HEAD'])}",
        f"git_dirty={'yes' if git_value(['status', '--short'], '') else 'no'}",
        f"archive={archive_name}",
        f"runtime_dir={runtime_dir}",
        f"stock_sdcard_dir={stock_sdcard_dir if stock_sdcard_dir is not None else 'disabled'}",
        "payload_root=sdcard-root",
        f"stock_files={stock_files}",
        f"stock_bytes={stock_bytes}",
        f"copied_files={copied_files}",
        f"copied_bytes={copied_bytes}",
        "",
        "[top-level]",
        "Emu/ (from stock payload when present)",
        "RetroArch/ (from stock payload when present)",
        "miyoo/app/MainUI",
        "miyoo/app/MainUI.stock",
        "miyoo/lib/",
        "plumos/",
        *EMPTY_USER_DIRS,
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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

    stock_files = 0
    stock_bytes = 0
    stock_sdcard_dir: Path | None = None
    if not args.no_stock_payload:
        stock_sdcard_dir = args.stock_sdcard_dir
        stock_files, stock_bytes = copy_stock_payload(stock_sdcard_dir, output_dir)
        extra_files, extra_bytes = preserve_stock_mainui(output_dir)
        stock_files += extra_files
        stock_bytes += extra_bytes

    files, bytes_total = copy_tree(runtime_plumos, output_dir / "plumos")
    copy_file(wrapper, output_dir / "miyoo/app/MainUI")
    (output_dir / "miyoo/app/MainUI").chmod(0o755)
    ensure_empty_dirs(output_dir)
    copy_notice_files(output_dir)
    write_readme(output_dir / "README.txt")
    write_manifest(
        output_dir / "manifest.txt",
        runtime_dir,
        stock_sdcard_dir,
        stock_files,
        stock_bytes,
        stock_files + files + 1,
        stock_bytes + bytes_total + wrapper.stat().st_size,
        archive.name,
    )

    errors = verify_payload(output_dir, require_stock_payload=not args.no_stock_payload)
    if errors:
        for error in errors:
            print(f"error: {error}", file=os.sys.stderr)
        raise SystemExit(1)

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
