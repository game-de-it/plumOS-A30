#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
FRONTEND_SRC="${ROOT_DIR}/src/frontend/plumos_frontend.c"
SCAN_SRC="${ROOT_DIR}/src/frontend/plumos_library_scan.c"
TEXT_UI_SRC="${ROOT_DIR}/src/frontend/plumos_text_ui.c"
CONTROLLER_UI_SRC="${ROOT_DIR}/src/frontend/plumos_controller_ui.c"
BOOT_SPLASH_SRC="${ROOT_DIR}/src/frontend/plumos_boot_splash.c"
SAFE_HOTKEYD_SRC="${ROOT_DIR}/src/input/plumos_safe_hotkeyd.c"
PACKAGE_DIR="${ROOT_DIR}/package/frontend/plumos"
DIST_DIR="${ROOT_DIR}/dist/plumos-frontend"
BIN_DIR="${DIST_DIR}/plumos/bin"
LIB_DIR="${DIST_DIR}/plumos/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-frontend"
FRONTEND_OUT="${BIN_DIR}/plumos-frontend"
SCAN_OUT="${BIN_DIR}/plumos-library-scan"
TEXT_UI_OUT="${BIN_DIR}/plumos-text-ui"
CONTROLLER_UI_OUT="${BIN_DIR}/plumos-controller-ui"
CONTROLLER_MALI_OUT="${BIN_DIR}/plumos-controller-ui-mali.bin"
CONTROLLER_MALI_WRAPPER="${BIN_DIR}/plumos-controller-ui-mali"
BOOT_SPLASH_OUT="${BIN_DIR}/plumos-boot-splash"
SAFE_HOTKEYD_OUT="${BIN_DIR}/plumos-safe-hotkeyd"
MANIFEST="${DOC_DIR}/manifest.txt"
PREFETCH_THUMBNAIL_CACHE="${PLUMOS_PREFETCH_THUMBNAIL_CACHE:-auto}"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"
READELF="${READELF:-arm-linux-gnueabihf-readelf}"

mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"
if [ -d "$PACKAGE_DIR" ]; then
  cp -a "${PACKAGE_DIR}/." "${DIST_DIR}/plumos/"
fi

prefetch_thumbnail_cache() {
  local scraper_cache_dir="${DIST_DIR}/plumos/share/frontend/artwork-scraper"
  local prefetch_script="${ROOT_DIR}/scripts/prefetch-thumbnail-scraper-cache.sh"

  case "$PREFETCH_THUMBNAIL_CACHE" in
    0|false|no|skip)
      echo "Skipping thumbnail scraper cache prefetch"
      return 0
      ;;
    1|true|yes|auto) ;;
    *)
      echo "error: invalid PLUMOS_PREFETCH_THUMBNAIL_CACHE=${PREFETCH_THUMBNAIL_CACHE}" >&2
      return 1
      ;;
  esac

  if "$prefetch_script" \
      --systems-json "${DIST_DIR}/plumos/config/frontend/systems.json" \
      --sources "${DIST_DIR}/plumos/config/frontend/scraper-sources.tsv" \
      --output "$scraper_cache_dir"; then
    return 0
  fi

  if [ "$PREFETCH_THUMBNAIL_CACHE" = "auto" ]; then
    echo "warning: thumbnail scraper cache prefetch failed; continuing because PLUMOS_PREFETCH_THUMBNAIL_CACHE=auto" >&2
    return 0
  fi
  return 1
}

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

build_mali_controller() {
  "$CC" \
    -std=gnu99 \
    -Os \
    -pipe \
    -static-libgcc \
    -march="${PLUMOS_MARCH:-armv7-a}" \
    -mfpu="${PLUMOS_MFPU:-neon-vfpv4}" \
    -mfloat-abi="${PLUMOS_MFLOAT_ABI:-hard}" \
    -Wall \
    -Wextra \
    -DPLUMOS_ENABLE_MALI_RENDERER=1 \
    -DPLUMOS_ENABLE_MALI_FREETYPE=1 \
    -DPLUMOS_ENABLE_MALI_PNG=1 \
    -I/usr/include/freetype2 \
    "$CONTROLLER_UI_SRC" \
    -o "$CONTROLLER_MALI_OUT" \
    -ldl \
    -lfreetype \
    -lpng \
    -lm

  "$STRIP" "$CONTROLLER_MALI_OUT" 2>/dev/null || true
  chmod 0755 "$CONTROLLER_MALI_OUT"

  copy_loader
  copy_deps "$CONTROLLER_MALI_OUT"
  for dep in \
    libc.so.6 \
    libdl.so.2 \
    libm.so.6 \
    libpthread.so.0 \
    librt.so.1 \
    libgcc_s.so.1 \
    libfreetype.so.6 \
    libz.so.1 \
    libpng16.so.16 \
    libbrotlidec.so.1 \
    libbrotlicommon.so.1; do
    install_lib_by_soname "$dep"
  done
  chmod 0755 "$LIB_DIR/ld-linux-armhf.so.3"

  cat > "$CONTROLLER_MALI_WRAPPER" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
PLUMOS_LIB="${PLUMOS_ROOT}/lib"
exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \
  --library-path "${PLUMOS_LIB}:/usr/lib:/lib" \
  "${PLUMOS_ROOT}/bin/plumos-controller-ui-mali.bin" --renderer mali "$@"
EOF
  chmod 0755 "$CONTROLLER_MALI_WRAPPER"
}

build_one "$FRONTEND_SRC" "$FRONTEND_OUT"
build_one "$SCAN_SRC" "$SCAN_OUT"
build_one "$TEXT_UI_SRC" "$TEXT_UI_OUT"
build_one "$CONTROLLER_UI_SRC" "$CONTROLLER_UI_OUT"
build_one "$BOOT_SPLASH_SRC" "$BOOT_SPLASH_OUT"
build_one "$SAFE_HOTKEYD_SRC" "$SAFE_HOTKEYD_OUT"
build_mali_controller
prefetch_thumbnail_cache

sha256sum \
  "$FRONTEND_OUT" \
  "$SCAN_OUT" \
  "$TEXT_UI_OUT" \
  "$CONTROLLER_UI_OUT" \
  "$BOOT_SPLASH_OUT" \
  "$SAFE_HOTKEYD_OUT" \
  "$CONTROLLER_MALI_OUT" \
  "$CONTROLLER_MALI_WRAPPER" \
  "$LIB_DIR"/* > "${DOC_DIR}/plumos-frontend.sha256"

{
  echo "plumOS frontend prototype"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("$CC" --version | head -n 1)"
  echo
  file "$FRONTEND_OUT"
  file "$SCAN_OUT"
  file "$TEXT_UI_OUT"
  file "$CONTROLLER_UI_OUT"
  file "$BOOT_SPLASH_OUT"
  file "$SAFE_HOTKEYD_OUT"
  file "$CONTROLLER_MALI_OUT"
  echo
  echo "== plumOS frontend =="
  "$READELF" -h "$FRONTEND_OUT"
  echo
  echo "== plumOS library scan =="
  "$READELF" -h "$SCAN_OUT"
  echo
  echo "== plumOS text UI =="
  "$READELF" -h "$TEXT_UI_OUT"
  echo
  echo "== plumOS controller UI =="
  "$READELF" -h "$CONTROLLER_UI_OUT"
  echo
  echo "== plumOS boot splash =="
  "$READELF" -h "$BOOT_SPLASH_OUT"
  echo
  echo "== plumOS safe hotkey daemon =="
  "$READELF" -h "$SAFE_HOTKEYD_OUT"
  echo
  echo "== plumOS controller UI Mali =="
  "$READELF" -h "$CONTROLLER_MALI_OUT"
  echo
  echo "== Mali interpreter =="
  "$READELF" -l "$CONTROLLER_MALI_OUT" | grep -A1 "INTERP" || true
  echo
  echo "== Mali needed =="
  "$READELF" -d "$CONTROLLER_MALI_OUT" | grep "NEEDED" || true
  echo
  echo "== bundled dynamic libs =="
  find "$LIB_DIR" -maxdepth 1 \( -type f -o -type l \) | sort | xargs -r -n1 basename
} > "$MANIFEST"

printf 'Built %s\n' "$FRONTEND_OUT"
printf 'Built %s\n' "$SCAN_OUT"
printf 'Built %s\n' "$TEXT_UI_OUT"
printf 'Built %s\n' "$CONTROLLER_UI_OUT"
printf 'Built %s\n' "$SAFE_HOTKEYD_OUT"
printf 'Built %s\n' "$CONTROLLER_MALI_OUT"
printf 'Wrapper %s\n' "$CONTROLLER_MALI_WRAPPER"
printf 'Manifest %s\n' "$MANIFEST"
