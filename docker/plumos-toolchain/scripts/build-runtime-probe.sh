#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/probe/plumos_runtime_probe.c"
INPUT_COMPARE_SRC="${ROOT_DIR}/src/probe/plumos_input_compare.c"
SHM_WATCH_SRC="${ROOT_DIR}/src/probe/plumos_shm_watch.c"
DIST_DIR="${ROOT_DIR}/dist/plumos-runtime-probe"
BIN_DIR="${DIST_DIR}/plumos/bin"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-runtime-probe"
OUT="${BIN_DIR}/plumos-runtime-probe"
INPUT_COMPARE_OUT="${BIN_DIR}/plumos-input-compare"
SHM_WATCH_OUT="${BIN_DIR}/plumos-shm-watch"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"

mkdir -p "$BIN_DIR" "$DOC_DIR"

build_one() {
  local src="$1"
  local out="$2"

  "$CC" \
    -std=gnu99 \
    -Os \
    -pipe \
    -static \
    -march="${PLUMOS_MARCH:-armv7-a}" \
    -mfpu="${PLUMOS_MFPU:-neon-vfpv4}" \
    -mfloat-abi="${PLUMOS_MFLOAT_ABI:-hard}" \
    -Wall \
    -Wextra \
    "$src" \
    -o "$out"

  "$STRIP" "$out" 2>/dev/null || true
  chmod 0755 "$out"
}

build_one "$SRC" "$OUT"
build_one "$INPUT_COMPARE_SRC" "$INPUT_COMPARE_OUT"
build_one "$SHM_WATCH_SRC" "$SHM_WATCH_OUT"

sha256sum "$OUT" "$INPUT_COMPARE_OUT" "$SHM_WATCH_OUT" > "${DOC_DIR}/plumos-runtime-probe.sha256"

{
  echo "plumOS runtime probe"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$OUT"
  file "$INPUT_COMPARE_OUT"
  file "$SHM_WATCH_OUT"
  echo
  echo "== plumOS runtime probe =="
  arm-linux-gnueabihf-readelf -h "$OUT"
  echo
  echo "== plumOS input compare =="
  arm-linux-gnueabihf-readelf -h "$INPUT_COMPARE_OUT"
  echo
  echo "== plumOS shm watch =="
  arm-linux-gnueabihf-readelf -h "$SHM_WATCH_OUT"
} > "$MANIFEST"

printf 'Built %s\n' "$OUT"
printf 'Built %s\n' "$INPUT_COMPARE_OUT"
printf 'Built %s\n' "$SHM_WATCH_OUT"
printf 'Manifest %s\n' "$MANIFEST"
