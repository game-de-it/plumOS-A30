#!/usr/bin/env python3
"""Audit and optionally clean a plumOS release SD-root staging directory."""

from __future__ import annotations

import argparse
import shutil
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TARGET = ROOT / "dist/plumos-release-sdroot"
DEFAULT_AUDIT_ROOT = ROOT / "artifacts/release-sdroot-audit"

EMPTY_PLACEHOLDER_DIRS = ["Bios", "Images", "Imgs", "Roms", "Saves"]

CLEAR_EXACT_PATHS = [
    ".pvnm",
    ".system.json.log",
    "RApp/backups",
    "plumos/backups",
    "plumos/artifacts",
    "plumos/logs",
    "plumos/run",
    "plumos/tmp",
    "plumos/test-bios-cfg",
    "plumos/retroarch/logs",
    "plumos/retroarch/home/.config/retroarch/saves",
    "plumos/retroarch/home/.config/retroarch/states",
    "plumos/ssh/etc/dropbear_ed25519_host_key",
    "plumos/ssh/etc/dropbear_ed25519_host_key.pub",
    "plumos/ssh/run/dropbear.pid",
    "Emu/PS/.pcsx/memcards",
    "Emu/PS/.pcsx/sstates",
    "Emu/Shoot/.retroarch/saves",
    "Emu/Shoot/.retroarch/states",
]

CLEAR_GLOBS = [
    "**/.DS_Store",
    "**/*.log",
    "**/*.pid",
    "**/*.tmp",
    "**/*~",
    "**/*.bak",
    "**/*.bak-*",
    "**/*.prev-*",
    "**/content_history.lpl",
    "**/content_image_history.lpl",
    "**/content_music_history.lpl",
    "**/playlists/logs",
    "plumos/config/**/*.before-*",
    "plumos/config/**/*.pre-*",
    "plumos/config/**/*.bak*",
    "plumos/ssh.backup-*",
]

SAVE_LIKE_SYSTEM_SUFFIXES = {
    ".brm",
    ".fs",
    ".hi",
    ".nv",
    ".sav",
    ".srm",
}

BIOS_LIKE_SUFFIXES = {
    ".bin",
    ".bios",
    ".pce",
    ".rom",
    ".sfc",
    ".zip",
}

RECREATE_DIRS = [
    "plumos/logs",
    "plumos/run",
    "plumos/tmp",
    "plumos/ssh/log",
    "plumos/ssh/run",
    "plumos/retroarch/logs",
    "plumos/retroarch/home/.config/retroarch/saves",
    "plumos/retroarch/home/.config/retroarch/states",
    "Emu/PS/.pcsx/memcards",
]


@dataclass(frozen=True)
class Finding:
    severity: str
    category: str
    path: Path
    detail: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit dist/plumos-release-sdroot for release blockers."
    )
    parser.add_argument(
        "target",
        nargs="?",
        type=Path,
        default=DEFAULT_TARGET,
        help="SD-root staging directory to audit.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Move clear blockers into a quarantine directory.",
    )
    parser.add_argument(
        "--quarantine-dir",
        type=Path,
        default=None,
        help="Quarantine directory used with --clean.",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=None,
        help="Optional TSV report path.",
    )
    return parser.parse_args()


def rel(path: Path, root: Path) -> Path:
    return path.relative_to(root)


def is_inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def compact_paths(paths: set[Path]) -> list[Path]:
    ordered = sorted(paths, key=lambda p: (len(p.parts), str(p)))
    compact: list[Path] = []
    for path in ordered:
        if any(is_inside(path, existing) for existing in compact):
            continue
        compact.append(path)
    return sorted(compact)


def add_existing(paths: set[Path], root: Path, relative_path: str) -> None:
    path = root / relative_path
    if path.exists() or path.is_symlink():
        if path.is_dir() and not any(path.rglob("*")):
            return
        paths.add(path)


def collect_clear_blockers(root: Path) -> list[Finding]:
    paths: set[Path] = set()
    for relative_path in CLEAR_EXACT_PATHS:
        add_existing(paths, root, relative_path)
    for pattern in CLEAR_GLOBS:
        for path in root.glob(pattern):
            if path.exists() or path.is_symlink():
                paths.add(path)

    shoot_system = root / "Emu/Shoot/.retroarch/system"
    if shoot_system.exists():
        for path in shoot_system.rglob("*"):
            if path.is_file() and path.suffix.lower() in SAVE_LIKE_SYSTEM_SUFFIXES:
                paths.add(path)

    findings: list[Finding] = []
    for path in compact_paths(paths):
        findings.append(
            Finding(
                severity="blocker",
                category="generated_or_user_state",
                path=rel(path, root),
                detail="remove from release package",
            )
        )
    return findings


def placeholder_warnings(root: Path) -> list[Finding]:
    findings: list[Finding] = []
    for name in EMPTY_PLACEHOLDER_DIRS:
        path = root / name
        if not path.exists():
            findings.append(
                Finding("warning", "placeholder", Path(name), "placeholder directory is missing")
            )
            continue
        files = [p for p in path.rglob("*") if p.is_file()]
        non_readme = [p for p in files if p.name != "README.txt"]
        if non_readme:
            findings.append(
                Finding(
                    "warning",
                    "placeholder",
                    Path(name),
                    f"contains {len(non_readme)} non-placeholder files",
                )
            )
    return findings


def bios_warnings(root: Path) -> list[Finding]:
    findings: list[Finding] = []
    candidates: list[Path] = []
    for base in [
        root / "Emu/PS/bios",
        root / "Emu/Shoot/.retroarch/system",
        root / "RetroArch/.retroarch/system",
        root / "plumos/retroarch/system",
    ]:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in BIOS_LIKE_SUFFIXES:
                candidates.append(path)
    if candidates:
        findings.append(
            Finding(
                "warning",
                "bios_policy",
                Path("."),
                f"{len(candidates)} BIOS/system/data-like files found outside top-level Bios",
            )
        )
    return findings


def ssh_warnings(root: Path) -> list[Finding]:
    findings: list[Finding] = []
    password_hash = root / "plumos/ssh/etc/password.hash"
    if not password_hash.exists():
        findings.append(
            Finding(
                "blocker",
                "ssh_policy",
                Path("plumos/ssh/etc/password.hash"),
                "missing SSH password hash file",
            )
        )
    authorized_keys = root / "plumos/ssh/etc/authorized_keys"
    if authorized_keys.exists():
        text = authorized_keys.read_text(encoding="utf-8", errors="replace")
        key_lines = [
            line
            for line in text.splitlines()
            if line.strip().startswith(("ssh-rsa", "ssh-ed25519", "ecdsa-sha2-"))
        ]
        if key_lines:
            findings.append(
                Finding(
                    "warning",
                    "ssh_policy",
                    rel(authorized_keys, root),
                    f"contains {len(key_lines)} public key line(s); OK for private test archive, not for public release",
                )
            )
    return findings


def collect_findings(root: Path) -> list[Finding]:
    return [
        *collect_clear_blockers(root),
        *placeholder_warnings(root),
        *bios_warnings(root),
        *ssh_warnings(root),
    ]


def write_report(path: Path, findings: list[Finding]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["severity\tcategory\tpath\tdetail"]
    lines.extend(
        f"{item.severity}\t{item.category}\t{item.path.as_posix()}\t{item.detail}"
        for item in findings
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def move_to_quarantine(root: Path, quarantine_dir: Path, findings: list[Finding]) -> int:
    blockers = [
        root / finding.path
        for finding in findings
        if finding.severity == "blocker"
    ]
    moved = 0
    for path in compact_paths({p for p in blockers if p.exists() or p.is_symlink()}):
        destination = quarantine_dir / rel(path, root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() or destination.is_symlink():
            if destination.is_dir() and not destination.is_symlink():
                shutil.rmtree(destination)
            else:
                destination.unlink()
        shutil.move(str(path), str(destination))
        moved += 1

    for relative_path in RECREATE_DIRS:
        (root / relative_path).mkdir(parents=True, exist_ok=True)
    return moved


def main() -> int:
    args = parse_args()
    target = args.target.resolve()
    if not target.exists():
        raise SystemExit(f"missing release sdroot: {target}")
    if not target.is_dir():
        raise SystemExit(f"not a directory: {target}")

    findings = collect_findings(target)
    if args.report is None:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        report = DEFAULT_AUDIT_ROOT / stamp / "release-sdroot-preflight.tsv"
    else:
        report = args.report
    write_report(report, findings)

    moved = 0
    if args.clean:
        quarantine = args.quarantine_dir
        if quarantine is None:
            quarantine = report.parent / "quarantine"
        moved = move_to_quarantine(target, quarantine.resolve(), findings)
        findings = collect_findings(target)
        write_report(report, findings)

    counts: dict[tuple[str, str], int] = {}
    for finding in findings:
        key = (finding.severity, finding.category)
        counts[key] = counts.get(key, 0) + 1

    print(f"target={target}")
    print(f"report={report}")
    if args.clean:
        print(f"quarantined={moved}")
    for (severity, category), count in sorted(counts.items()):
        print(f"{severity}.{category}={count}")
    return 1 if any(f.severity == "blocker" for f in findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
