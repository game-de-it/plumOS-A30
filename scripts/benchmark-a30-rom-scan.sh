#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
COUNT="${1:-1000}"
SYSTEM="${PLUMOS_BENCH_SYSTEM:-nes}"
BENCH_ROOT="${PLUMOS_BENCH_ROOT:-/mnt/SDCARD/plumos/tmp/rom-scan-bench}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [DUMMY_FILE_COUNT]

Defaults:
  DUMMY_FILE_COUNT     ${COUNT}
  PLUMOS_BENCH_SYSTEM ${SYSTEM}
  PLUMOS_BENCH_ROOT   ${BENCH_ROOT}

The benchmark creates dummy ROMs only under PLUMOS_BENCH_ROOT and removes that
directory before each run.
EOF
}

case "${1:-}" in
  -h|--help)
    usage
    exit 0
    ;;
esac

if ! [[ "$COUNT" =~ ^[0-9]+$ ]] || [ "$COUNT" -le 0 ]; then
  echo "error: DUMMY_FILE_COUNT must be a positive integer" >&2
  exit 1
fi

remote_script=$(cat <<'EOS'
set -eu

count="$1"
system="$2"
bench_root="$3"
rom_dir="$bench_root/Roms/FC"
systems_src="/mnt/SDCARD/plumos/config/frontend/systems.json"
systems_dst="$bench_root/plumos/config/frontend/systems.json"
scanner="/mnt/SDCARD/plumos/bin/plumos-library-scan"

cleanup() {
  rm -rf "$bench_root"
}
trap cleanup EXIT

if [ ! -x "$scanner" ]; then
  echo "error: missing scanner: $scanner" >&2
  exit 1
fi
if [ ! -f "$systems_src" ]; then
  echo "error: missing systems.json: $systems_src" >&2
  exit 1
fi

rm -rf "$bench_root"
mkdir -p "$rom_dir" "$bench_root/plumos/config/frontend"
cp "$systems_src" "$systems_dst"

i=1
while [ "$i" -le "$count" ]; do
  name=$(printf '%04d.nes' "$i")
  : > "$rom_dir/$name"
  i=$((i + 1))
done

PLUMOS_SDCARD_ROOT="$bench_root" \
PLUMOS_ROOT="$bench_root/plumos" \
"$scanner" --on-enter "$system"
EOS
)

"${ROOT_DIR}/scripts/run-a30.sh" "sh -s -- '$COUNT' '$SYSTEM' '$BENCH_ROOT'" <<<"$remote_script"
