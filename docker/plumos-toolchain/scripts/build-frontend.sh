#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
FRONTEND_SRC="${ROOT_DIR}/src/frontend/plumos_frontend.c"
SCAN_SRC="${ROOT_DIR}/src/frontend/plumos_library_scan.c"
TEXT_UI_SRC="${ROOT_DIR}/src/frontend/plumos_text_ui.c"
PACKAGE_DIR="${ROOT_DIR}/package/frontend/plumos"
DIST_DIR="${ROOT_DIR}/dist/plumos-frontend"
BIN_DIR="${DIST_DIR}/plumos/bin"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-frontend"
FRONTEND_OUT="${BIN_DIR}/plumos-frontend"
SCAN_OUT="${BIN_DIR}/plumos-library-scan"
TEXT_UI_OUT="${BIN_DIR}/plumos-text-ui"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"

mkdir -p "$BIN_DIR" "$DOC_DIR"
if [ -d "$PACKAGE_DIR" ]; then
  cp -a "${PACKAGE_DIR}/." "${DIST_DIR}/plumos/"
fi

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

build_one "$FRONTEND_SRC" "$FRONTEND_OUT"
build_one "$SCAN_SRC" "$SCAN_OUT"
build_one "$TEXT_UI_SRC" "$TEXT_UI_OUT"

sha256sum "$FRONTEND_OUT" "$SCAN_OUT" "$TEXT_UI_OUT" > "${DOC_DIR}/plumos-frontend.sha256"

{
  echo "plumOS frontend prototype"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$FRONTEND_OUT"
  file "$SCAN_OUT"
  file "$TEXT_UI_OUT"
  echo
  echo "== plumOS frontend =="
  arm-linux-gnueabihf-readelf -h "$FRONTEND_OUT"
  echo
  echo "== plumOS library scan =="
  arm-linux-gnueabihf-readelf -h "$SCAN_OUT"
  echo
  echo "== plumOS text UI =="
  arm-linux-gnueabihf-readelf -h "$TEXT_UI_OUT"
} > "$MANIFEST"

printf 'Built %s\n' "$FRONTEND_OUT"
printf 'Built %s\n' "$SCAN_OUT"
printf 'Built %s\n' "$TEXT_UI_OUT"
printf 'Manifest %s\n' "$MANIFEST"
