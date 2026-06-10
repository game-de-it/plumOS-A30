#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SYSTEM="${PLUMOS_CRC_BENCH_SYSTEM:-nes}"
WORKERS="${PLUMOS_CRC_BENCH_WORKERS:-1 2 3 4 5}"
LIMIT="${PLUMOS_CRC_BENCH_LIMIT:-100}"
TOOL="${PLUMOS_CRC_BENCH_TOOL:-auto}"
ROM_ROOT="${PLUMOS_CRC_BENCH_ROM_ROOT:-/mnt/SDCARD/Roms}"
SYSTEMS_JSON="${PLUMOS_SYSTEMS_JSON:-"${ROOT_DIR}/package/frontend/plumos/config/frontend/systems.json"}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Benchmark A30 CRC/read worker counts for one frontend system. The benchmark
prints aggregate timing only; it does not print ROM filenames.

Options:
  --system ID      Frontend system id. Default: ${SYSTEM}
  --workers LIST   Space/comma separated worker counts. Default: ${WORKERS}
  --limit N        Max files to benchmark. Default: ${LIMIT}
  --tool TOOL      auto, crc32, cksum, or read. Default: ${TOOL}
                   auto prefers crc32, then cksum, then raw read.
  --rom-root PATH  A30 ROM root. Default: ${ROM_ROOT}
  -h, --help       Show this help.

Environment:
  PLUMOS_SYSTEMS_JSON            Host-side systems.json source.
  PLUMOS_CRC_BENCH_SYSTEM        Default --system.
  PLUMOS_CRC_BENCH_WORKERS       Default --workers.
  PLUMOS_CRC_BENCH_LIMIT         Default --limit.
  PLUMOS_CRC_BENCH_TOOL          Default --tool.
  PLUMOS_CRC_BENCH_ROM_ROOT      Default --rom-root.
  PLUMOS_CRC_BENCH_DROP_CACHES=1 Drop Linux page cache before each worker run.
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --system) SYSTEM="${2:-}"; shift 2 ;;
    --workers) WORKERS="${2:-}"; shift 2 ;;
    --limit) LIMIT="${2:-}"; shift 2 ;;
    --tool) TOOL="${2:-}"; shift 2 ;;
    --rom-root) ROM_ROOT="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[ -n "$SYSTEM" ] || die "--system is required"
[[ "$LIMIT" =~ ^[0-9]+$ ]] || die "--limit must be a positive integer"
[ "$LIMIT" -gt 0 ] || die "--limit must be greater than 0"
case "$TOOL" in
  auto|crc32|cksum|read) ;;
  *) die "invalid --tool: $TOOL" ;;
esac

WORKERS="$(printf '%s\n' "$WORKERS" | tr ',' ' ')"
for worker in $WORKERS; do
  [[ "$worker" =~ ^[0-9]+$ ]] || die "invalid worker count: $worker"
  [ "$worker" -gt 0 ] || die "worker count must be greater than 0"
done

system_meta="$(
  python3 - "$SYSTEMS_JSON" "$SYSTEM" <<'PY'
import json
import sys

path, wanted = sys.argv[1], sys.argv[2]
with open(path, encoding="utf-8") as handle:
    data = json.load(handle)

for system in data.get("systems", []):
    if system.get("id") != wanted:
        continue
    aliases = [alias["name"] for alias in system.get("directory_aliases", []) if alias.get("name")]
    extensions = [ext.lower().lstrip(".") for ext in system.get("extensions", []) if ext]
    print("\t".join([system["id"], " ".join(aliases), " ".join(extensions)]))
    break
else:
    raise SystemExit(f"missing system in systems.json: {wanted}")
PY
)"

IFS=$'\t' read -r SYSTEM_ID ALIASES EXTENSIONS <<<"$system_meta"
[ -n "$ALIASES" ] || die "system has no directory aliases: $SYSTEM"
[ -n "$EXTENSIONS" ] || die "system has no extensions: $SYSTEM"

"${ROOT_DIR}/scripts/run-a30.sh" \
  "sh -s -- '$SYSTEM_ID' '$ALIASES' '$EXTENSIONS' '$WORKERS' '$LIMIT' '$TOOL' '$ROM_ROOT' '${PLUMOS_CRC_BENCH_DROP_CACHES:-0}'" <<'EOS'
set -eu

system_id="$1"
aliases="$2"
extensions="$3"
workers_list="$4"
limit="$5"
tool_requested="$6"
rom_root="$7"
drop_caches="${8:-0}"

tmp_list="/tmp/plumos-crc-bench-${system_id}-$$.list"
cleanup() {
  rm -f "$tmp_list"
}
trap cleanup EXIT INT TERM

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

choose_tool() {
  case "$tool_requested" in
    crc32)
      has_cmd crc32 || return 1
      printf 'crc32\n'
      ;;
    cksum)
      has_cmd cksum || return 1
      printf 'cksum\n'
      ;;
    read)
      printf 'read\n'
      ;;
    auto)
      if has_cmd crc32; then
        printf 'crc32\n'
      elif has_cmd cksum; then
        printf 'cksum\n'
      else
        printf 'read\n'
      fi
      ;;
  esac
}

bench_tool="$(choose_tool)" || {
  echo "error: requested tool is unavailable: $tool_requested" >&2
  exit 1
}

matches_ext() {
  file="$1"
  name="${file##*/}"
  case "$name" in
    *.*) ext="${name##*.}" ;;
    *) return 1 ;;
  esac
  ext="$(printf '%s' "$ext" | tr '[:upper:]' '[:lower:]')"
  for wanted in $extensions; do
    [ "$ext" = "$wanted" ] && return 0
  done
  return 1
}

count=0
: >"$tmp_list"
for alias in $aliases; do
  dir="${rom_root%/}/$alias"
  [ -d "$dir" ] || continue
  find "$dir" -type f | while IFS= read -r file; do
    matches_ext "$file" || continue
    printf '%s\n' "$file"
  done
done | sort | while IFS= read -r file; do
  [ "$count" -lt "$limit" ] || break
  printf '%s\n' "$file" >>"$tmp_list"
  count=$((count + 1))
done

file_count="$(wc -l <"$tmp_list" | tr -d ' ')"
if [ "$file_count" -eq 0 ]; then
  echo "error: no matching ROM files found for system=$system_id under $rom_root" >&2
  exit 1
fi

stat_size() {
  file="$1"
  if has_cmd stat; then
    stat -c '%s' "$file" 2>/dev/null && return 0
  fi
  printf '0\n'
}

total_bytes=0
while IFS= read -r file; do
  size="$(stat_size "$file")"
  case "$size" in
    ''|*[!0-9]*) size=0 ;;
  esac
  total_bytes=$((total_bytes + size))
done <"$tmp_list"

run_one() {
  file="$1"
  case "$bench_tool" in
    crc32) crc32 "$file" >/dev/null ;;
    cksum) cksum "$file" >/dev/null ;;
    read) dd if="$file" of=/dev/null bs=1M >/dev/null 2>&1 ;;
  esac
}

now_seconds() {
  awk '{ print $1 }' /proc/uptime
}

printf 'system\ttool\tworkers\tfiles\tbytes\tseconds\tfiles_per_sec\tmib_per_sec\n'
for workers in $workers_list; do
  if [ "$drop_caches" = "1" ]; then
    sync || true
    echo 3 >/proc/sys/vm/drop_caches 2>/dev/null || true
  fi
  start="$(now_seconds)"
  active=0
  while IFS= read -r file; do
    run_one "$file" &
    active=$((active + 1))
    if [ "$active" -ge "$workers" ]; then
      wait
      active=0
    fi
  done <"$tmp_list"
  wait
  stop="$(now_seconds)"
  awk -v system="$system_id" \
      -v tool="$bench_tool" \
      -v workers="$workers" \
      -v files="$file_count" \
      -v bytes="$total_bytes" \
      -v start="$start" \
      -v stop="$stop" '
    BEGIN {
      seconds = stop - start
      if (seconds <= 0) seconds = 0.01
      mib = bytes / 1048576
      printf "%s\t%s\t%d\t%d\t%d\t%.3f\t%.2f\t%.2f\n",
        system, tool, workers, files, bytes, seconds, files / seconds, mib / seconds
    }'
done
EOS
