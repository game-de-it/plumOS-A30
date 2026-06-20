#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SYSTEM="${PLUMOS_SCRAPER_BENCH_SYSTEM:-gba}"
MODE="${PLUMOS_SCRAPER_BENCH_MODE:-plan}"
LIMIT="${PLUMOS_SCRAPER_BENCH_LIMIT:-}"
KIND="${PLUMOS_SCRAPER_BENCH_KIND:-}"
REPLACE_EXISTING="${PLUMOS_SCRAPER_BENCH_REPLACE_EXISTING:-0}"
SAMPLE_INTERVAL="${PLUMOS_SCRAPER_BENCH_SAMPLE_INTERVAL:-1}"
DISK="${PLUMOS_SCRAPER_BENCH_DISK:-mmcblk0p1}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Benchmark the packaged thumbnail scraper on an A30 without printing ROM names.

Options:
  --system ID          Frontend system id. Default: ${SYSTEM}
  --mode plan|fetch    Scraper mode to run. Default: ${MODE}
  --limit N            Optional scraper limit.
  --kind KIND          Optional scraper kind, e.g. Named_Titles.
  --replace-existing   Pass --replace-existing to fetch.
  --sample-interval N  Resource sample interval in seconds. Default: ${SAMPLE_INTERVAL}
  --disk DEV           /proc/diskstats device. Default: ${DISK}
  -h, --help           Show this help.
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --system) SYSTEM="${2:-}"; shift 2 ;;
    --mode) MODE="${2:-}"; shift 2 ;;
    --limit) LIMIT="${2:-}"; shift 2 ;;
    --kind) KIND="${2:-}"; shift 2 ;;
    --replace-existing) REPLACE_EXISTING=1; shift ;;
    --sample-interval) SAMPLE_INTERVAL="${2:-}"; shift 2 ;;
    --disk) DISK="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[ -n "$SYSTEM" ] || die "--system is required"
case "$MODE" in
  plan|fetch) ;;
  *) die "--mode must be plan or fetch" ;;
esac
if [ -n "$LIMIT" ]; then
  [[ "$LIMIT" =~ ^[0-9]+$ ]] || die "--limit must be numeric"
fi
[[ "$SAMPLE_INTERVAL" =~ ^[0-9]+$ ]] || die "--sample-interval must be numeric"
[ "$SAMPLE_INTERVAL" -gt 0 ] || die "--sample-interval must be greater than 0"
case "$REPLACE_EXISTING" in
  0|1) ;;
  *) die "invalid replace-existing value: $REPLACE_EXISTING" ;;
esac

"${ROOT_DIR}/scripts/run-a30.sh" \
  "sh -s -- '$SYSTEM' '$MODE' '$LIMIT' '$KIND' '$REPLACE_EXISTING' '$SAMPLE_INTERVAL' '$DISK'" <<'REMOTE'
set -eu

system_id="$1"
mode="$2"
limit="$3"
kind="$4"
replace_existing="$5"
sample_interval="$6"
disk="$7"

PATH="/mnt/SDCARD/plumos/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
export PATH

scraper="/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper"
[ -x "$scraper" ] || {
  echo "error: scraper is not executable: $scraper" >&2
  exit 1
}

tmp_prefix="/tmp/plumos-scraper-bench-${system_id}-${mode}-$$"
out_file="${tmp_prefix}.out"
err_file="${tmp_prefix}.err"
sample_file="${tmp_prefix}.samples"

cleanup() {
  rm -f "$out_file" "$err_file" "$sample_file"
}
trap cleanup EXIT INT TERM

now_seconds() {
  awk '{ print $1 }' /proc/uptime
}

cpu_snapshot() {
  awk '/^cpu / {
    total = 0
    for (i = 2; i <= NF; i++) total += $i
    idle = $5
    if (NF >= 6) idle += $6
    printf "%s %s\n", total, idle
    exit
  }' /proc/stat
}

mem_field() {
  key="$1"
  awk -v key="$key" '$1 == key ":" { print $2; found = 1; exit } END { if (!found) print 0 }' /proc/meminfo
}

disk_snapshot() {
  awk -v dev="$disk" '
    $3 == dev {
      printf "%s %s %s %s %s\n", $4, $6, $8, $10, $13
      found = 1
      exit
    }
    END {
      if (!found) print "0 0 0 0 0"
    }' /proc/diskstats
}

if [ "$mode" = "fetch" ]; then
  set -- "$scraper" --fetch --system "$system_id"
else
  set -- "$scraper" --plan --system "$system_id"
fi
[ -z "$limit" ] || set -- "$@" --limit "$limit"
[ -z "$kind" ] || set -- "$@" --kind "$kind"
if [ "$replace_existing" = "1" ]; then
  set -- "$@" --replace-existing
fi

mem_total_before="$(mem_field MemTotal)"
mem_free_before="$(mem_field MemFree)"
cpu_before="$(cpu_snapshot)"
disk_before="$(disk_snapshot)"
start="$(now_seconds)"
: >"$sample_file"

"$@" >"$out_file" 2>"$err_file" &
bench_pid="$!"

monitor_samples() {
  while kill -0 "$bench_pid" 2>/dev/null; do
    printf '%s\t%s\n' "$(now_seconds)" "$(mem_field MemFree)" >>"$sample_file"
    sleep "$sample_interval"
  done
}

monitor_samples &
monitor_pid="$!"

set +e
wait "$bench_pid"
status="$?"
set -e
stop="$(now_seconds)"
kill "$monitor_pid" 2>/dev/null || true
wait "$monitor_pid" 2>/dev/null || true

cpu_after="$(cpu_snapshot)"
disk_after="$(disk_snapshot)"
mem_free_after="$(mem_field MemFree)"

read -r cpu_total_before cpu_idle_before <<EOF
$cpu_before
EOF
read -r cpu_total_after cpu_idle_after <<EOF
$cpu_after
EOF
read -r disk_reads_before disk_read_sectors_before disk_writes_before disk_write_sectors_before disk_io_ms_before <<EOF
$disk_before
EOF
read -r disk_reads_after disk_read_sectors_after disk_writes_after disk_write_sectors_after disk_io_ms_after <<EOF
$disk_after
EOF

mem_free_min="$mem_free_before"
sample_count=0
if [ -s "$sample_file" ]; then
  mem_free_min="$(awk 'NR == 1 || $2 < min { min = $2 } END { print min + 0 }' "$sample_file")"
  sample_count="$(wc -l <"$sample_file" | tr -d ' ')"
fi
[ "$mem_free_after" -lt "$mem_free_min" ] && mem_free_min="$mem_free_after"
mem_used_peak=$((mem_total_before - mem_free_min))

awk -v sysid="$system_id" \
    -v mode="$mode" \
    -v status="$status" \
    -v start="$start" \
    -v stop="$stop" \
    -v ctb="$cpu_total_before" \
    -v cib="$cpu_idle_before" \
    -v cta="$cpu_total_after" \
    -v cia="$cpu_idle_after" \
    -v mt="$mem_total_before" \
    -v mfb="$mem_free_before" \
    -v mfa="$mem_free_after" \
    -v mfm="$mem_free_min" \
    -v mup="$mem_used_peak" \
    -v disk="$disk" \
    -v drb="$disk_reads_before" \
    -v drsb="$disk_read_sectors_before" \
    -v dwb="$disk_writes_before" \
    -v dwsb="$disk_write_sectors_before" \
    -v diob="$disk_io_ms_before" \
    -v dra="$disk_reads_after" \
    -v drsa="$disk_read_sectors_after" \
    -v dwa="$disk_writes_after" \
    -v dwsa="$disk_write_sectors_after" \
    -v dioa="$disk_io_ms_after" \
    -v samples="$sample_count" '
  BEGIN {
    elapsed = stop - start
    if (elapsed < 0) elapsed = 0
    total_delta = cta - ctb
    idle_delta = cia - cib
    cpu_busy = 0
    if (total_delta > 0) cpu_busy = (total_delta - idle_delta) * 100 / total_delta
    read_kib = (drsa - drsb) * 512 / 1024
    write_kib = (dwsa - dwsb) * 512 / 1024
    io_ms = dioa - diob
    printf "benchmark\tsystem\tmode\tstatus\telapsed_sec\tcpu_busy_pct\tmem_total_kb\tmem_free_before_kb\tmem_free_after_kb\tmem_free_min_kb\tmem_used_peak_kb\tdisk\tread_ops\twrite_ops\tread_kib\twrite_kib\tio_ms\tsamples\n"
    printf "benchmark\t%s\t%s\t%d\t%.3f\t%.2f\t%d\t%d\t%d\t%d\t%d\t%s\t%d\t%d\t%.1f\t%.1f\t%d\t%d\n",
      sysid, mode, status, elapsed, cpu_busy, mt, mfb, mfa, mfm, mup, disk,
      dra - drb, dwa - dwb, read_kib, write_kib, io_ms, samples
  }'

echo "scraper_stdout_begin"
cat "$out_file"
echo "scraper_stdout_end"
if [ -s "$err_file" ]; then
  echo "scraper_stderr_begin"
  cat "$err_file"
  echo "scraper_stderr_end"
fi

exit "$status"
REMOTE
