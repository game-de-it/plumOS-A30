#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=${ROOT_DIR:-/workspace}
SRC="${ROOT_DIR}/src/probe/plumos_sdl1_fbcon_probe.c"
DIST_DIR="${ROOT_DIR}/dist/plumos-sdl1-fbcon-probe"
BIN_DIR="${DIST_DIR}/plumos/bin"
APP_DIR="${DIST_DIR}/plumos/apps/sdl1-fbcon-probe"
APP_BIN_DIR="${APP_DIR}/bin"
APP_LIB_DIR="${APP_DIR}/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-sdl1-fbcon-probe"
OUT_BIN="${APP_BIN_DIR}/plumos-sdl1-fbcon-probe.bin"
OUT_WRAPPER="${BIN_DIR}/plumos-sdl1-fbcon-probe"
MANIFEST="${DOC_DIR}/manifest.txt"

CC="${CC:-arm-linux-gnueabihf-gcc}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"
READELF="${READELF:-arm-linux-gnueabihf-readelf}"
SDL_CONFIG="${SDL_CONFIG:-sdl-config}"

find_target_lib() {
  local name=$1
  local dir
  for dir in \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf/pulseaudio \
    /usr/arm-linux-gnueabihf/lib \
    /lib \
    /usr/lib; do
    if [ -e "${dir}/${name}" ]; then
      printf '%s/%s\n' "${dir}" "${name}"
      return 0
    fi
  done
  return 1
}

copy_if_present() {
  local src=$1
  local dst=$2
  local soname=${3:-$(basename "${src}")}
  local real_src real_base

  [ -e "${src}" ] || return 0
  real_src=$(readlink -f "${src}")
  real_base=$(basename "${real_src}")
  mkdir -p "${dst}"
  if [ ! -f "${dst}/${real_base}" ]; then
    install -m 0644 "${real_src}" "${dst}/${real_base}"
  fi
  if [ "${soname}" != "${real_base}" ] && [ ! -f "${dst}/${soname}" ]; then
    cp -f "${dst}/${real_base}" "${dst}/${soname}"
    chmod 0644 "${dst}/${soname}"
  fi
}

copy_dep_tree() {
  local elf=$1
  local lib_dir=$2
  local dep path

  [ -f "${elf}" ] || return 0
  "${READELF}" -d "${elf}" 2>/dev/null |
    awk -F"[][]" '/NEEDED/ {print $2}' |
    while IFS= read -r dep; do
      [ -n "${dep}" ] || continue
      case "${dep}" in
        libSDL-1.2.so*|libSDL.so*)
          continue
          ;;
      esac
      path=$(find_target_lib "${dep}" || true)
      if [ -z "${path}" ]; then
        echo "warning: could not locate runtime dependency ${dep}" >&2
        continue
      fi
      if [ ! -f "${lib_dir}/${dep}" ]; then
        copy_if_present "${path}" "${lib_dir}" "${dep}"
        copy_dep_tree "$(readlink -f "${path}")" "${lib_dir}"
      fi
    done
}

copy_runtime_deps() {
  local binary=$1
  local lib_dir=$2
  local lib path

  mkdir -p "${lib_dir}"
  for lib in ld-linux-armhf.so.3 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 \
    librt.so.1 libgcc_s.so.1; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}" "${lib}"
    fi
  done
  copy_dep_tree "${binary}" "${lib_dir}"
  chmod 0755 "${lib_dir}/ld-linux-armhf.so.3" 2>/dev/null || true
}

rm -rf "${DIST_DIR}"
mkdir -p "${BIN_DIR}" "${APP_BIN_DIR}" "${APP_LIB_DIR}" "${DOC_DIR}"

"${CC}" \
  -std=gnu99 \
  -O2 \
  -pipe \
  -march="${PLUMOS_MARCH:-armv7-a}" \
  -mfpu="${PLUMOS_MFPU:-neon-vfpv4}" \
  -mfloat-abi="${PLUMOS_MFLOAT_ABI:-hard}" \
  -Wall \
  -Wextra \
  $("${SDL_CONFIG}" --cflags) \
  "${SRC}" \
  -o "${OUT_BIN}" \
  $("${SDL_CONFIG}" --libs)

"${STRIP}" "${OUT_BIN}" 2>/dev/null || true
chmod 0755 "${OUT_BIN}"
copy_runtime_deps "${OUT_BIN}" "${APP_LIB_DIR}"

cat >"${OUT_WRAPPER}" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
APP_ROOT="${PLUMOS_ROOT}/apps/sdl1-fbcon-probe"
APP_LIB="${APP_ROOT}/lib"
LOADER="${APP_LIB}/ld-linux-armhf.so.3"
if [ ! -x "${LOADER}" ]; then
  LOADER="${PLUMOS_ROOT}/lib/ld-linux-armhf.so.3"
fi
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-fbcon}"
export SDL_FBDEV="${SDL_FBDEV:-/dev/fb0}"
export SDL_NOMOUSE="${SDL_NOMOUSE:-1}"
exec "${LOADER}" \
  --library-path "${APP_LIB}:/mnt/SDCARD/miyoo/lib:/usr/lib:/lib:${PLUMOS_ROOT}/lib" \
  "${APP_ROOT}/bin/plumos-sdl1-fbcon-probe.bin" "$@"
EOF
chmod 0755 "${OUT_WRAPPER}"

sha256sum "${OUT_BIN}" "${OUT_WRAPPER}" "${APP_LIB_DIR}"/* >"${DOC_DIR}/plumos-sdl1-fbcon-probe.sha256"

{
  echo "plumOS SDL1 fbcon probe"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cc: $("${CC}" --version | head -n 1)"
  echo "sdl-config-version: $("${SDL_CONFIG}" --version)"
  echo "sdl-config-cflags: $("${SDL_CONFIG}" --cflags)"
  echo "sdl-config-libs: $("${SDL_CONFIG}" --libs)"
  echo
  file "${OUT_BIN}"
  echo
  "${READELF}" -h "${OUT_BIN}"
  echo
  echo "== interpreter =="
  "${READELF}" -l "${OUT_BIN}" | grep -A1 "INTERP" || true
  echo
  echo "== needed =="
  "${READELF}" -d "${OUT_BIN}" | grep "NEEDED" || true
  echo
  echo "== bundled libs =="
  find "${APP_LIB_DIR}" -maxdepth 1 \( -type f -o -type l \) | sort | xargs -r -n1 basename
} >"${MANIFEST}"

printf 'Built %s\n' "${OUT_BIN}"
printf 'Wrapper %s\n' "${OUT_WRAPPER}"
printf 'Manifest %s\n' "${MANIFEST}"
