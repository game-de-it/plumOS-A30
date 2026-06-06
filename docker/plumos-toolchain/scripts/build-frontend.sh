#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/frontend/plumos_frontend.c"
DIST_DIR="${ROOT_DIR}/dist/plumos-frontend"
BIN_DIR="${DIST_DIR}/plumos/bin"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-frontend"
OUT="${BIN_DIR}/plumos-frontend"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"

mkdir -p "$BIN_DIR" "$DOC_DIR"

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
  "$SRC" \
  -o "$OUT"

"$STRIP" "$OUT" 2>/dev/null || true
chmod 0755 "$OUT"

sha256sum "$OUT" > "${DOC_DIR}/plumos-frontend.sha256"

{
  echo "plumOS frontend prototype"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$OUT"
  echo
  arm-linux-gnueabihf-readelf -h "$OUT"
} > "$MANIFEST"

printf 'Built %s\n' "$OUT"
printf 'Manifest %s\n' "$MANIFEST"
