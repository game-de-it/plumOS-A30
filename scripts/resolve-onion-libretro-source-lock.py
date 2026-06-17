#!/usr/bin/env python3
"""Resolve Onion libretro prebuilt cores to source commits by build window."""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

FIELDS = [
    "id",
    "plumos_recipe",
    "plumos_class",
    "version_policy",
    "onion_display_version",
    "onion_binary_date",
    "embedded_build_date",
    "lock_date",
    "lock_date_source",
    "onion_builder_repo",
    "onion_builder_ref",
    "onion_builder_subdir",
    "onion_builder_makefile",
    "onion_builder_args",
    "resolved_source_commit",
    "resolved_source_date",
    "resolved_source_subject",
    "plumos_ref",
    "resolution_status",
    "notes",
]

MONTHS = {
    "Jan": 1,
    "Feb": 2,
    "Mar": 3,
    "Apr": 4,
    "May": 5,
    "Jun": 6,
    "Jul": 7,
    "Aug": 8,
    "Sep": 9,
    "Oct": 10,
    "Nov": 11,
    "Dec": 12,
}

MONTH_DATE_RE = re.compile(
    r"\b("
    + "|".join(MONTHS)
    + r")\s+([0-9]{1,2})\s+([0-9]{4})(?:[, ]+\s*([0-9]{1,2}):([0-9]{2})(?::([0-9]{2}))?)?",
    re.ASCII,
)
ISO_DATE_RE = re.compile(r"\b(20[0-9]{2})-([0-9]{2})-([0-9]{2})(?:[T ][0-9:.,+-Z]*)?")
FULL_SHA_RE = re.compile(r"^[0-9a-f]{40}$", re.ASCII)
MAX_EMBEDDED_DATE_AGE_DAYS = 180

SOURCE_OVERRIDES = {
    # Onion commit 4741519 explicitly says fake08 was updated to aebd6b9.
    "fake08": {
        "commit": "aebd6b9648d1c450910ad79c78f9280dfbd20b25",
        "reason": "onion_subject_commit_hint",
    },
    # The latest FBNeo master at the Onion binary date no longer contains the
    # libretro build path used by the Onion recipe. Use the latest libretro
    # Makefile-era commit before the Onion binary date.
    "fbneo": {
        "commit": "bce2754588b17ce7a69acc5faf4ce15bc1a4b80e",
        "reason": "latest_libretro_makefile_commit_before_onion_binary",
    },
    # The Onion-window source marks Makefile.libretro as unmaintained. Pin the
    # current libretro fork commit that keeps the libretro makefile buildable.
    "pcsx_rearmed": {
        "commit": "d26eaee5c8fb47c1832b8bf32c1358d625da8a02",
        "reason": "onion_window_makefile_libretro_unmaintained",
    },
    # The 2.7.0-era ScummVM source does not contain the current libretro backend
    # path. Keep a buildable libretro backend commit instead of shipping Onion's
    # binary.
    "scummvm": {
        "commit": "660e13b0764fe2be39b6d723345ecabfbb318cc5",
        "reason": "onion_window_libretro_backend_missing",
    },
    # The nesbox 0.80-era tree references a now-unfetchable submodule. The
    # libretro wrapper repo keeps TIC-80 as a submodule with a buildable backend.
    "tic80": {
        "repo": "https://github.com/libretro/TIC-80.git",
        "commit": "e7f48a30ea3e205810366aad910a2985edcd1f58",
        "reason": "libretro_wrapper_repo_buildable",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a TSV that maps Onion prebuilt libretro cores to an "
            "upstream source commit at the inferred Onion build window."
        )
    )
    parser.add_argument(
        "--inventory",
        default=ROOT / "docs/libretro-core-version-inventory.tsv",
        type=Path,
        help="Input TSV from scripts/inventory-libretro-core-versions.py.",
    )
    parser.add_argument(
        "--onion-core-dir",
        default=ROOT / "artifacts/onion/Onion/static/build/RetroArch/.retroarch/cores",
        type=Path,
        help="Onion prebuilt core directory.",
    )
    parser.add_argument(
        "--cache-dir",
        default=ROOT / "build/onion-source-lock/git",
        type=Path,
        help="Bare git cache directory used to resolve source refs.",
    )
    parser.add_argument(
        "--include-missing-plumos",
        action="store_true",
        help="Include Onion prebuilt cores that currently have no plumOS recipe.",
    )
    parser.add_argument(
        "--no-fetch",
        action="store_true",
        help="Reuse cached git mirrors without fetching network updates.",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Debug helper: resolve at most this many rows.",
    )
    return parser.parse_args()


def run(cmd: list[str], *, check: bool = False, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def parse_datetime(value: str) -> datetime | None:
    text = value.strip()
    if not text or text == "-":
        return None
    try:
        normalized = text.replace("Z", "+00:00")
        return datetime.fromisoformat(normalized).astimezone(timezone.utc)
    except ValueError:
        pass
    match = ISO_DATE_RE.search(text)
    if match:
        year, month, day = (int(match.group(i)) for i in range(1, 4))
        return datetime(year, month, day, tzinfo=timezone.utc)
    return None


def format_dt(value: datetime | None) -> str:
    if value is None:
        return "-"
    return value.astimezone(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle, dialect="excel-tab"))


def clean_cell(value: str) -> str:
    cleaned = " ".join(str(value).replace("\t", " ").splitlines()).strip()
    return cleaned if cleaned else "-"


def strings_lines(path: Path) -> list[str]:
    result = run(["strings", str(path)])
    if result.returncode != 0:
        return []
    return result.stdout.splitlines()


def embedded_dates(path: Path, binary_date: datetime | None) -> list[datetime]:
    dates: set[datetime] = set()
    for line in strings_lines(path):
        for match in MONTH_DATE_RE.finditer(line):
            month = MONTHS[match.group(1)]
            day = int(match.group(2))
            year = int(match.group(3))
            hour = int(match.group(4) or 0)
            minute = int(match.group(5) or 0)
            second = int(match.group(6) or 0)
            dates.add(datetime(year, month, day, hour, minute, second, tzinfo=timezone.utc))
        for match in ISO_DATE_RE.finditer(line):
            year, month, day = (int(match.group(i)) for i in range(1, 4))
            dates.add(datetime(year, month, day, tzinfo=timezone.utc))

    plausible: list[datetime] = []
    for item in dates:
        if item.year < 2018:
            continue
        if binary_date is not None and item > binary_date:
            continue
        plausible.append(item)
    return sorted(plausible)


def choose_lock_date(row: dict[str, str], core_path: Path) -> tuple[datetime | None, str, str]:
    binary_date = parse_datetime(row.get("onion_binary_date", ""))
    candidates = embedded_dates(core_path, binary_date) if core_path.exists() else []
    if candidates:
        chosen = candidates[-1]
        embedded = ";".join(format_dt(item) for item in candidates[-5:])
        if binary_date is not None and (binary_date - chosen).days > MAX_EMBEDDED_DATE_AGE_DAYS:
            return binary_date, "onion_binary_commit_date", embedded
        return chosen, "embedded_binary_date", embedded
    return binary_date, "onion_binary_commit_date", "-"


def commit_metadata(repo_dir: Path, commit: str) -> tuple[str, str, str, str, str]:
    exists = run(["git", "-C", str(repo_dir), "rev-parse", "--verify", f"{commit}^{{commit}}"])
    if exists.returncode != 0:
        return commit, "-", "-", "override_ref_not_found", clean_cell(exists.stderr)[0:180]
    full_commit = exists.stdout.strip()
    meta = run(["git", "-C", str(repo_dir), "show", "-s", "--format=%cI%x09%s", full_commit])
    if meta.returncode != 0:
        return full_commit, "-", "-", "metadata_failed", clean_cell(meta.stderr)[0:180]
    date, subject = (meta.stdout.rstrip("\n").split("\t", 1) + [""])[:2]
    return full_commit, date, subject, "resolved", "source_override"


def safe_repo_dir(cache_dir: Path, repo: str) -> Path:
    digest = hashlib.sha256(repo.encode("utf-8")).hexdigest()[:16]
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", repo.removesuffix(".git").split("/")[-1]) or "repo"
    return cache_dir / f"{slug}-{digest}.git"


def ensure_repo(repo: str, cache_dir: Path, no_fetch: bool) -> tuple[Path | None, str | None]:
    if not repo or repo == "-":
        return None, "missing_repo"
    path = safe_repo_dir(cache_dir, repo)
    cache_dir.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        if no_fetch:
            return None, "missing_cache"
        result = run(["git", "clone", "--bare", "--filter=blob:none", repo, str(path)])
        if result.returncode != 0:
            return None, "clone_failed:" + clean_cell(result.stderr)[0:180]
    elif not no_fetch:
        result = run(
            [
                "git",
                "-C",
                str(path),
                "fetch",
                "--filter=blob:none",
                "--prune",
                "origin",
                "+refs/heads/*:refs/remotes/origin/*",
                "+refs/tags/*:refs/tags/*",
            ]
        )
        if result.returncode != 0:
            return None, "fetch_failed:" + clean_cell(result.stderr)[0:180]
    return path, None


def resolve_ref(repo_dir: Path, ref: str, lock_date: datetime | None) -> tuple[str, str, str, str, str]:
    if not ref or ref == "-":
        return "-", "-", "-", "missing_ref", "no Onion builder ref"

    candidates: list[str] = []
    if ref == "HEAD":
        candidates.append("HEAD")
    elif FULL_SHA_RE.match(ref):
        candidates.append(ref)
    else:
        candidates.extend(
            [
                f"refs/remotes/origin/{ref}",
                f"refs/heads/{ref}",
                f"refs/tags/{ref}",
                ref,
            ]
        )

    chosen_ref = ""
    for candidate in candidates:
        exists = run(["git", "-C", str(repo_dir), "rev-parse", "--verify", f"{candidate}^{{commit}}"])
        if exists.returncode == 0 and exists.stdout.strip():
            chosen_ref = candidate
            break
    if not chosen_ref:
        return "-", "-", "-", "ref_not_found", f"ref={ref}"

    if FULL_SHA_RE.match(ref):
        commit = ref
    elif lock_date is not None:
        result = run(
            [
                "git",
                "-C",
                str(repo_dir),
                "rev-list",
                "-1",
                f"--before={format_dt(lock_date)}",
                chosen_ref,
            ]
        )
        commit = result.stdout.strip() if result.returncode == 0 else ""
        if not commit:
            return "-", "-", "-", "no_commit_before_lock_date", f"ref={ref} lock_date={format_dt(lock_date)}"
    else:
        result = run(["git", "-C", str(repo_dir), "rev-parse", f"{chosen_ref}^{{commit}}"])
        commit = result.stdout.strip() if result.returncode == 0 else ""
        if not commit:
            return "-", "-", "-", "resolve_failed", f"ref={ref}"

    meta = run(["git", "-C", str(repo_dir), "show", "-s", "--format=%cI%x09%s", commit])
    if meta.returncode != 0:
        return commit, "-", "-", "metadata_failed", clean_cell(meta.stderr)[0:180]
    date, subject = (meta.stdout.rstrip("\n").split("\t", 1) + [""])[:2]
    return commit, date, subject, "resolved", f"ref={ref} resolved_from={chosen_ref}"


def resolve_with_fallbacks(
    repo_dir: Path,
    refs: list[str],
    lock_date: datetime | None,
) -> tuple[str, str, str, str, str]:
    attempts: list[str] = []
    seen: set[str] = set()
    for ref in refs:
        if not ref or ref == "-" or ref in seen:
            continue
        seen.add(ref)
        commit, date, subject, status, notes = resolve_ref(repo_dir, ref, lock_date)
        if status == "resolved":
            if attempts:
                notes = f"{notes}; fallback_after={','.join(attempts)}"
            return commit, date, subject, status, notes
        attempts.append(f"{ref}:{status}")
    if attempts:
        return "-", "-", "-", attempts[-1].split(":", 1)[1], ";".join(attempts)
    return "-", "-", "-", "missing_ref", "no candidate refs"


def main() -> int:
    args = parse_args()
    rows = read_tsv(args.inventory)
    out_rows: list[dict[str, str]] = []
    resolved_count = 0

    for row in rows:
        if row.get("onion_prebuilt") != "yes":
            continue
        if row.get("plumos_recipe") != "yes" and not args.include_missing_plumos:
            continue
        if args.limit and len(out_rows) >= args.limit:
            break

        core_id = row["id"]
        core_path = args.onion_core_dir / f"{core_id}_libretro.so"
        lock_date, lock_source, embedded = choose_lock_date(row, core_path)
        repo = row.get("onion_builder_repo", "-")
        if not repo or repo == "-":
            repo = row.get("plumos_repo", "-")
        override = SOURCE_OVERRIDES.get(core_id)
        if override and override.get("repo"):
            repo = override["repo"]
        ref = row.get("onion_builder_ref", "-")
        fallback_refs = [ref, row.get("plumos_ref", "-"), "main", "master"]
        repo_dir, repo_error = ensure_repo(repo, args.cache_dir, args.no_fetch)

        if repo_error:
            commit = date = subject = "-"
            status = repo_error
            notes = "-"
        else:
            assert repo_dir is not None
            if override:
                commit, date, subject, status, notes = commit_metadata(repo_dir, override["commit"])
                notes = f"{notes}; reason={override['reason']}"
            else:
                commit, date, subject, status, notes = resolve_with_fallbacks(repo_dir, fallback_refs, lock_date)
            if status == "resolved":
                resolved_count += 1

        out_rows.append(
            {
                "id": core_id,
                "plumos_recipe": row.get("plumos_recipe", "-"),
                "plumos_class": row.get("plumos_class", "-"),
                "version_policy": row.get("version_policy", "-"),
                "onion_display_version": row.get("onion_display_version", "-"),
                "onion_binary_date": row.get("onion_binary_date", "-"),
                "embedded_build_date": embedded,
                "lock_date": format_dt(lock_date),
                "lock_date_source": lock_source if lock_date else "missing",
                "onion_builder_repo": repo,
                "onion_builder_ref": ref,
                "onion_builder_subdir": row.get("onion_builder_subdir", "-"),
                "onion_builder_makefile": row.get("onion_builder_makefile", "-"),
                "onion_builder_args": row.get("onion_builder_args", "-"),
                "resolved_source_commit": commit,
                "resolved_source_date": date,
                "resolved_source_subject": subject,
                "plumos_ref": row.get("plumos_ref", "-"),
                "resolution_status": status,
                "notes": notes,
            }
        )

    import sys

    writer = csv.DictWriter(sys.stdout, fieldnames=FIELDS, dialect="excel-tab", lineterminator="\n")
    writer.writeheader()
    for out_row in out_rows:
        writer.writerow({key: clean_cell(out_row.get(key, "")) for key in FIELDS})
    print(
        f"resolved={resolved_count} total={len(out_rows)} cache={args.cache_dir}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
