#!/usr/bin/env python3
"""Assemble a plumOS A30 developer/toolchain source package."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import tarfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

REQUIRED_SOURCE_PATHS = [
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES.ja.md",
    "TODO.md",
    "docs/docker-toolchain.md",
    "docs/release-artifacts.md",
    "docs/runtime-package.md",
    "docs/sdroot-package.md",
    "docs/ssh-bringup.md",
    "docker/plumos-toolchain/Dockerfile",
    "docker/plumos-toolchain/libretro-core-recipes.tsv",
    "scripts/docker-build.sh",
    "scripts/build-ssh-kit.sh",
    "scripts/build-release-bundle.py",
    "scripts/build-runtime-package.py",
    "scripts/build-sdroot-package.py",
    "scripts/build-dev-package.py",
]


@dataclass(frozen=True)
class SourceCopyResult:
    files: int
    bytes_total: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build dist/plumos-dev-package from the current source tree."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "dist/plumos-dev-package",
    )
    parser.add_argument(
        "--archive",
        type=Path,
        default=None,
        help="Archive path. Defaults to <output-dir>.tar.gz.",
    )
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="Allow packaging a dirty working tree. The manifest records git_dirty=yes.",
    )
    parser.add_argument("--no-archive", action="store_true")
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
            "working tree is dirty; commit/stash changes or pass --allow-dirty for a preview package"
        )
    return status


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


def source_paths() -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        check=True,
        stdout=subprocess.PIPE,
    )
    return sorted(path.decode("utf-8") for path in result.stdout.split(b"\0") if path)


def copy_source_tree(source_dir: Path) -> SourceCopyResult:
    files = 0
    bytes_total = 0
    for rel_text in source_paths():
        rel = Path(rel_text)
        if rel.parts and rel.parts[0] in {".git", "build", "dist", "artifacts"}:
            continue
        src = ROOT / rel
        if not src.exists() and not src.is_symlink():
            continue
        dst = source_dir / rel
        copy_file(src, dst)
        files += 1
        if src.is_file():
            bytes_total += src.stat().st_size
    return SourceCopyResult(files=files, bytes_total=bytes_total)


def verify_source_payload(source_dir: Path) -> list[str]:
    errors: list[str] = []
    for rel in REQUIRED_SOURCE_PATHS:
        if not (source_dir / rel).exists():
            errors.append(f"missing source path: {rel}")
    return errors


def write_root_readme(package_dir: Path) -> None:
    (package_dir / "README.md").write_text(
        """# plumOS A30 developer package

This package is a source and Docker toolchain snapshot for rebuilding plumOS A30 artifacts.

## Layout

- `source/`: repository source snapshot, excluding generated `build/`, `dist/`, and `artifacts/`
- `manifest.txt`: package provenance
- `sha256sum.txt`: payload checksums

## First build

```sh
cd source
./scripts/docker-build.sh image
./scripts/docker-build.sh frontend
./scripts/docker-build.sh libretro-cores
```

End-user SD-root package generation is documented in `source/docs/sdroot-package.md`.
Runtime package generation is documented in `source/docs/runtime-package.md`.
Docker/toolchain details are documented in `source/docs/docker-toolchain.md`.
plumOS-authored files are MIT-licensed; bundled third-party notices are in
`source/THIRD_PARTY_NOTICES.md`.
""",
        encoding="utf-8",
    )


def write_manifest(package_dir: Path, copy_result: SourceCopyResult, status: str, archive_name: str) -> None:
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = [
        "plumOS A30 developer package",
        f"build_time={build_time}",
        f"git_commit={git_value(['rev-parse', 'HEAD'])}",
        f"git_branch={git_value(['rev-parse', '--abbrev-ref', 'HEAD'])}",
        f"git_dirty={'yes' if status else 'no'}",
        f"archive={archive_name}",
        "source_root=source",
        "source_mode=git-ls-files-working-tree",
        f"source_files={copy_result.files}",
        f"source_bytes={copy_result.bytes_total}",
        "",
        "[required-source-paths]",
        *REQUIRED_SOURCE_PATHS,
    ]
    if status:
        lines.extend(["", "[dirty-status]", status])
    (package_dir / "manifest.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


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
    status = ensure_clean_tree(args.allow_dirty)

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    source_dir = output_dir / "source"
    source_dir.mkdir()
    copy_result = copy_source_tree(source_dir)

    errors = verify_source_payload(source_dir)
    if errors:
        for error in errors:
            print(f"error: {error}", file=os.sys.stderr)
        raise SystemExit(1)

    write_root_readme(output_dir)
    write_manifest(output_dir, copy_result, status, archive.name)
    write_sha256(output_dir)
    if not args.no_archive:
        create_archive(output_dir, archive)

    print(f"package_dir={output_dir}")
    if not args.no_archive:
        print(f"archive={archive}")
        print(f"archive_sha256={sha256_file(archive)}")
    print(f"source_files={copy_result.files}")
    print(f"git_dirty={'yes' if status else 'no'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
