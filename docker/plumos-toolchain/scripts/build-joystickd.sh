#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/input/plumos_joystickd.c"
READER_SRC="${ROOT_DIR}/src/input/plumos_joystick_reader.c"
DIST_DIR="${ROOT_DIR}/dist/plumos-joystickd"
BIN_DIR="${DIST_DIR}/plumos/bin"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-joystickd"
OUT="${BIN_DIR}/plumos-joystickd"
READER_OUT="${BIN_DIR}/plumos-joystick-reader"
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
build_one "$READER_SRC" "$READER_OUT"

sha256sum "$OUT" "$READER_OUT" > "${DOC_DIR}/plumos-joystickd.sha256"

{
  echo "plumOS joystickd"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$OUT"
  file "$READER_OUT"
  echo
  echo "== plumOS joystickd =="
  arm-linux-gnueabihf-readelf -h "$OUT"
  echo
  echo "== plumOS joystick reader =="
  arm-linux-gnueabihf-readelf -h "$READER_OUT"
} > "$MANIFEST"

printf 'Built %s\n' "$OUT"
printf 'Built %s\n' "$READER_OUT"
printf 'Manifest %s\n' "$MANIFEST"
