#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/probe/plumos_mali_egl_probe.c"
DIST_DIR="${ROOT_DIR}/dist/plumos-mali-egl-probe"
BIN_DIR="${DIST_DIR}/plumos/bin"
LIB_DIR="${DIST_DIR}/plumos/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-mali-egl-probe"
OUT_BIN="${BIN_DIR}/plumos-mali-egl-probe.bin"
OUT_WRAPPER="${BIN_DIR}/plumos-mali-egl-probe"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"
READELF="${READELF:-arm-linux-gnueabihf-readelf}"

rm -rf "$DIST_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"

"$CC" \
  -std=gnu99 \
  -Os \
  -pipe \
  -march="${PLUMOS_MARCH:-armv7-a}" \
  -mfpu="${PLUMOS_MFPU:-neon-vfpv4}" \
  -mfloat-abi="${PLUMOS_MFLOAT_ABI:-hard}" \
  -Wall \
  -Wextra \
  "$SRC" \
  -o "$OUT_BIN" \
  -ldl

"$STRIP" "$OUT_BIN" 2>/dev/null || true
chmod 0755 "$OUT_BIN"

find_lib() {
  local soname="$1"
  local dir

  for dir in \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/arm-linux-gnueabihf/lib \
    /lib \
    /usr/lib; do
    if [ -e "$dir/$soname" ]; then
      readlink -f "$dir/$soname"
      return 0
    fi
  done
  return 1
}

install_lib_by_soname() {
  local soname="$1"
  local src
  local base

  if [ -e "$LIB_DIR/$soname" ]; then
    return 0
  fi
  src="$(find_lib "$soname")" || {
    echo "warning: dependency not found: $soname" >&2
    return 0
  }
  base="$(basename "$src")"
  install -m 0644 "$src" "$LIB_DIR/$base"
  if [ "$base" != "$soname" ]; then
    cp -f "$LIB_DIR/$base" "$LIB_DIR/$soname"
    chmod 0644 "$LIB_DIR/$soname"
  fi
}

copy_deps() {
  local elf="$1"
  local dep

  "$READELF" -d "$elf" 2>/dev/null | awk -F"[][]" "/NEEDED/ {print \$2}" | while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    install_lib_by_soname "$dep"
  done
}

copy_loader() {
  local loader

  for loader in \
    /lib/ld-linux-armhf.so.3 \
    /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 \
    /usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3; do
    if [ -e "$loader" ]; then
      install -m 0755 "$(readlink -f "$loader")" "$LIB_DIR/ld-linux-armhf.so.3"
      return 0
    fi
  done
  echo "error: armhf dynamic loader not found" >&2
  return 1
}

copy_loader
copy_deps "$OUT_BIN"

# libMali.so is loaded from the A30 rootfs, but its libc-side dependencies must
# resolve to the same bundled glibc family as the probe.
for dep in libc.so.6 libdl.so.2 libm.so.6 libpthread.so.0 librt.so.1 libgcc_s.so.1; do
  install_lib_by_soname "$dep"
done
chmod 0755 "$LIB_DIR/ld-linux-armhf.so.3"

cat > "$OUT_WRAPPER" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
PLUMOS_LIB="${PLUMOS_ROOT}/lib"
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  export LD_LIBRARY_PATH="${PLUMOS_LIB}:/usr/lib:/lib:${LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="${PLUMOS_LIB}:/usr/lib:/lib"
fi
exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \
  --library-path "${PLUMOS_LIB}:/usr/lib:/lib" \
  "${PLUMOS_ROOT}/bin/plumos-mali-egl-probe.bin" "$@"
EOF
chmod 0755 "$OUT_WRAPPER"

sha256sum "$OUT_BIN" "$OUT_WRAPPER" "$LIB_DIR"/* > "${DOC_DIR}/plumos-mali-egl-probe.sha256"

{
  echo "plumOS Mali EGL probe"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$OUT_BIN"
  echo
  "$READELF" -h "$OUT_BIN"
  echo
  echo "== interpreter =="
  "$READELF" -l "$OUT_BIN" | grep -A1 "INTERP" || true
  echo
  echo "== needed =="
  "$READELF" -d "$OUT_BIN" | grep "NEEDED" || true
  echo
  echo "== bundled libs =="
  find "$LIB_DIR" -maxdepth 1 \( -type f -o -type l \) | sort | xargs -r -n1 basename
} > "$MANIFEST"

printf 'Built %s\n' "$OUT_BIN"
printf 'Wrapper %s\n' "$OUT_WRAPPER"
printf 'Manifest %s\n' "$MANIFEST"
