#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/smoke/plumos_smoke.c"
OUT_DIR="${ROOT_DIR}/dist/docker-smoke"
OUT="${OUT_DIR}/plumos-smoke-armhf"
MANIFEST="${OUT}.manifest.txt"
SHA_FILE="${OUT}.sha256"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"

mkdir -p "$OUT_DIR"

"$CC" \
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

sha256sum "$OUT" > "$SHA_FILE"

{
  echo "plumOS Docker smoke build"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$OUT"
  echo
  arm-linux-gnueabihf-readelf -h "$OUT"
} > "$MANIFEST"

printf 'Built %s\n' "$OUT"
printf 'Manifest %s\n' "$MANIFEST"
