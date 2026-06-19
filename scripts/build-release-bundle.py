#!/usr/bin/env python3
"""Assemble GitHub Release-ready plumOS A30 artifacts."""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Copy runtime/developer packages into a release asset directory."
    )
    parser.add_argument(
        "--version",
        default=None,
        help="Release version used in asset names. Defaults to git describe --tags --always.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory. Defaults to dist/plumos-release-<version>.",
    )
    parser.add_argument(
        "--runtime-archive",
        type=Path,
        default=ROOT / "dist/plumos-runtime-package.tar.gz",
    )
    parser.add_argument(
        "--developer-archive",
        type=Path,
        default=ROOT / "dist/plumos-dev-package.tar.gz",
    )
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="Allow a dirty working tree. The release manifest records git_dirty=yes.",
    )
    return parser.parse_args()


def git_lines(args: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(ROOT), *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout.splitlines()


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


def git_status_porcelain() -> str:
    return "\n".join(git_lines(["status", "--porcelain"]))


def ensure_clean_tree(allow_dirty: bool) -> str:
    status = git_status_porcelain()
    if status and not allow_dirty:
        raise SystemExit(
            "working tree is dirty; commit/stash changes or pass --allow-dirty for a preview release"
        )
    return status


def default_version() -> str:
    return git_value(["describe", "--tags", "--always"])


def safe_version(value: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9._+-]+", "-", value.strip())
    return safe.strip("-") or "unknown"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_asset(src: Path, dst: Path) -> None:
    if not src.exists():
        raise SystemExit(f"missing release input archive: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def write_release_notes(path: Path, version: str, runtime_name: str, developer_name: str) -> None:
    path.write_text(
        f"""# plumOS A30 {version}

## Assets

- `{runtime_name}`: end-user SD-card runtime package.
- `{developer_name}`: developer source/toolchain package.
- `SHA256SUMS`: checksums for release assets and release metadata.
- `manifest.txt`: provenance for this release asset set.

## Install

End users should download only `{runtime_name}` unless they need to rebuild plumOS.

```sh
tar -xzf {runtime_name}
cd plumos-runtime-package
./install-plumos-runtime.sh /mnt/SDCARD
```

The runtime installer preserves user settings, logs, RetroArch saves/states, playlists, and standalone emulator state paths documented in `docs/runtime-package.md`.

## Developer Rebuild

Developers should use `{developer_name}` when they need the Docker toolchain, patches, recipes, and source snapshot used to rebuild artifacts.

```sh
tar -xzf {developer_name}
cd plumos-dev-package/source
./scripts/docker-build.sh image
```

## Excluded Data

ROMs, BIOS files, user save data, state files, network secrets, and generated build caches are not release assets.
""",
        encoding="utf-8",
    )


def write_manifest(path: Path, version: str, status: str, assets: list[Path]) -> None:
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = [
        "plumOS A30 release assets",
        f"version={version}",
        f"build_time={build_time}",
        f"git_commit={git_value(['rev-parse', 'HEAD'])}",
        f"git_branch={git_value(['rev-parse', '--abbrev-ref', 'HEAD'])}",
        f"git_dirty={'yes' if status else 'no'}",
        "",
        "[assets]",
    ]
    for asset in assets:
        lines.append(f"{asset.name}\tbytes={asset.stat().st_size}\tsha256={sha256_file(asset)}")
    if status:
        lines.extend(["", "[dirty-status]", status])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_sha256(path: Path, files: list[Path]) -> None:
    lines = [f"{sha256_file(item)}  {item.name}" for item in files]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    status = ensure_clean_tree(args.allow_dirty)
    version = safe_version(args.version or default_version())
    output_dir = args.output_dir or ROOT / f"dist/plumos-release-{version}"

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    runtime_name = f"plumos-a30-runtime-{version}.tar.gz"
    developer_name = f"plumos-a30-developer-{version}.tar.gz"
    runtime_out = output_dir / runtime_name
    developer_out = output_dir / developer_name
    copy_asset(args.runtime_archive, runtime_out)
    copy_asset(args.developer_archive, developer_out)

    notes = output_dir / "RELEASE_NOTES.md"
    manifest = output_dir / "manifest.txt"
    checksums = output_dir / "SHA256SUMS"
    write_release_notes(notes, version, runtime_name, developer_name)
    write_manifest(manifest, version, status, [runtime_out, developer_out])
    write_sha256(checksums, [runtime_out, developer_out, notes, manifest])

    print(f"release_dir={output_dir}")
    print(f"version={version}")
    print(f"runtime={runtime_out}")
    print(f"runtime_sha256={sha256_file(runtime_out)}")
    print(f"developer={developer_out}")
    print(f"developer_sha256={sha256_file(developer_out)}")
    print(f"git_dirty={'yes' if status else 'no'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
