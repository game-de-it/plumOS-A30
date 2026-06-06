#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
SRC="${ROOT_DIR}/src/probe/plumos_sdl2_probe.c"
SDL2_SOURCE="${PLUMOS_SDL2_SOURCE:-upstream}"
SDL2_RUNTIME_SCRIPT="${ROOT_DIR}/docker/plumos-toolchain/scripts/build-sdl2-runtime.sh"
SDL2_RUNTIME_DIST="${ROOT_DIR}/dist/plumos-sdl2-runtime"
SDL2_COMPAT_PREFIX="${ROOT_DIR}/build/sdl-upstream/sdl2-compat-install"
DIST_DIR="${ROOT_DIR}/dist/plumos-sdl2-probe"
BIN_DIR="${DIST_DIR}/plumos/bin"
LIB_DIR="${DIST_DIR}/plumos/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-sdl2-probe"
OUT_BIN="${BIN_DIR}/plumos-sdl2-probe.bin"
OUT_WRAPPER="${BIN_DIR}/plumos-sdl2-probe"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"
READELF="${READELF:-arm-linux-gnueabihf-readelf}"
PKG_CONFIG="${PKG_CONFIG:-pkg-config}"
SDL2_PKG="${SDL2_PKG:-}"

if [ "$SDL2_SOURCE" != "upstream" ]; then
  echo "error: only PLUMOS_SDL2_SOURCE=upstream is supported: $SDL2_SOURCE" >&2
  exit 2
fi

"$SDL2_RUNTIME_SCRIPT"
SDL2_PKG="${SDL2_PKG:-sdl2-compat}"
export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-${SDL2_COMPAT_PREFIX}/lib/pkgconfig}"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}"

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
  $("$PKG_CONFIG" --cflags "$SDL2_PKG") \
  "$SRC" \
  -o "$OUT_BIN" \
  $("$PKG_CONFIG" --libs "$SDL2_PKG")

"$STRIP" "$OUT_BIN" 2>/dev/null || true
chmod 0755 "$OUT_BIN"

find_lib() {
  local soname="$1"
  local dir

  for dir in \
    /lib/arm-linux-gnueabihf \
    /lib/arm-linux-gnueabihf/pulseaudio \
    "${SDL2_RUNTIME_DIST}/plumos/lib" \
    "${SDL2_COMPAT_PREFIX}/lib" \
    "${ROOT_DIR}/build/sdl-upstream/SDL3-install/lib" \
    /usr/lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf/pulseaudio \
    /usr/arm-linux-gnueabihf/lib; do
    if [ -e "$dir/$soname" ]; then
      readlink -f "$dir/$soname"
      return 0
    fi
  done
  return 1
}

copy_deps() {
  local elf="$1"
  local dep
  local src
  local base

  "$READELF" -d "$elf" 2>/dev/null | awk -F"[][]" "/NEEDED/ {print \$2}" | while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    if [ -e "$LIB_DIR/$dep" ]; then
      continue
    fi
    src="$(find_lib "$dep")" || {
      echo "warning: dependency not found: $dep" >&2
      continue
    }
    base="$(basename "$src")"
    install -m 0644 "$src" "$LIB_DIR/$base"
    if [ "$base" != "$dep" ]; then
      cp -f "$LIB_DIR/$base" "$LIB_DIR/$dep"
      chmod 0644 "$LIB_DIR/$dep"
    fi
    copy_deps "$src"
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
if [ "$SDL2_SOURCE" = "upstream" ]; then
  cp -f "${SDL2_RUNTIME_DIST}/plumos/lib/"* "$LIB_DIR/"
  chmod 0644 "$LIB_DIR"/*
  chmod 0755 "$LIB_DIR/ld-linux-armhf.so.3"
fi
copy_deps "$OUT_BIN"

cat > "$OUT_WRAPPER" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
PLUMOS_LIB="${PLUMOS_ROOT}/lib"
if [ "${PLUMOS_SDL2_DEFAULT_DUMMY:-1}" = "1" ]; then
  export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
fi
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  export LD_LIBRARY_PATH="${PLUMOS_LIB}:${LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="${PLUMOS_LIB}"
fi
exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \
  --library-path "${PLUMOS_LIB}" \
  "${PLUMOS_ROOT}/bin/plumos-sdl2-probe.bin" "$@"
EOF
chmod 0755 "$OUT_WRAPPER"

sha256sum "$OUT_BIN" "$OUT_WRAPPER" "$LIB_DIR"/* > "${DOC_DIR}/plumos-sdl2-probe.sha256"

{
  echo "plumOS SDL2 probe"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo "sdl2_source: ${SDL2_SOURCE}"
  echo "pkg-config-package: ${SDL2_PKG}"
  echo "pkg-config: $("$PKG_CONFIG" --modversion "$SDL2_PKG")"
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
