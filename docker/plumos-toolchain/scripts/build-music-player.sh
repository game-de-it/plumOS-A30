#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-music-player"}
DEPS_ROOT=${DEPS_ROOT:-"${BUILD_ROOT}/music-player/deps"}

MINIAUDIO_REPO=${MINIAUDIO_REPO:-https://github.com/mackron/miniaudio.git}
MINIAUDIO_REF=${MINIAUDIO_REF:-9634bedb5b5a2ca38c1ee7108a9358a4e233f14d}
MINIAUDIO_RAW_URL=${MINIAUDIO_RAW_URL:-https://raw.githubusercontent.com/mackron/miniaudio/${MINIAUDIO_REF}/miniaudio.h}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

msg() {
  printf '[music-player] %s\n' "$*" >&2
}

tool_path() {
  command -v "$1" 2>/dev/null || printf '%s\n' "$1"
}

find_target_lib() {
  local name=$1
  local dir
  for dir in \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
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
  local real_src
  local real_base

  [ -e "${src}" ] || return 0
  mkdir -p "${dst}"
  real_src=$(readlink -f "${src}")
  real_base=$(basename "${real_src}")
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
  local dep
  local path

  [ -f "${elf}" ] || return 0
  "${READELF}" -d "${elf}" 2>/dev/null |
    awk -F"[][]" '/NEEDED/ {print $2}' |
    while IFS= read -r dep; do
      [ -n "${dep}" ] || continue
      case "${dep}" in
        ld-linux-armhf.so.3|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|libgcc_s.so.1)
          continue
          ;;
        libEGL.so*|libGLESv2.so*|libMali.so*|libGL.so*)
          continue
          ;;
      esac
      path=$(find_target_lib "${dep}" || true)
      if [ -z "${path}" ]; then
        msg "warning: could not locate runtime dependency ${dep}"
        continue
      fi
      if [ ! -f "${lib_dir}/${dep}" ]; then
        copy_if_present "${path}" "${lib_dir}" "${dep}"
        copy_dep_tree "$(readlink -f "${path}")" "${lib_dir}"
      fi
    done
}

fetch_miniaudio() {
  local include_dir="${DEPS_ROOT}/miniaudio"
  local header="${include_dir}/miniaudio.h"
  mkdir -p "${include_dir}"
  if [ ! -f "${header}" ]; then
    msg "fetching miniaudio ${MINIAUDIO_REF}"
    curl -LfsS "${MINIAUDIO_RAW_URL}" -o "${header}"
  fi
  printf '%s\n' "${include_dir}"
}

write_launcher() {
  local out=$1
  cat >"${out}" <<'EOF'
#!/bin/sh
set -u

PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
APP_ROOT="${PLUMOS_MUSIC_PLAYER_ROOT:-${PLUMOS_ROOT}/apps/music-player}"
STATE_DIR="${PLUMOS_MUSIC_PLAYER_STATE:-${PLUMOS_ROOT}/state/apps/music-player}"
LOG_DIR="${PLUMOS_MUSIC_PLAYER_LOG_DIR:-${PLUMOS_ROOT}/logs/apps}"
LOADER="${PLUMOS_ROOT}/lib/ld-linux-armhf.so.3"
JOYSTICKD_OPTS="${PLUMOS_MUSIC_PLAYER_JOYSTICKD_OPTS:---device-mode xbox --trigger-mode buttons --shoulder-layout user --function-button guide}"
joystickd_pid=
cleanup_done=0

mkdir -p "${STATE_DIR}" "${LOG_DIR}" 2>/dev/null || true

cleanup() {
  [ "${cleanup_done}" = 1 ] && return
  cleanup_done=1
  if [ -n "${joystickd_pid:-}" ] && kill -0 "${joystickd_pid}" >/dev/null 2>&1; then
    kill -TERM "${joystickd_pid}" >/dev/null 2>&1 || true
    sleep 1
    kill -KILL "${joystickd_pid}" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM HUP

if [ ! -x "${APP_ROOT}/bin/plumos-music-player.bin" ]; then
  echo "error: missing music player binary: ${APP_ROOT}/bin/plumos-music-player.bin" >&2
  exit 127
fi
if [ ! -x "${LOADER}" ]; then
  echo "error: missing dynamic loader: ${LOADER}" >&2
  exit 127
fi

if [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ]; then
  "${PLUMOS_ROOT}/bin/plumos-joystickd" ${JOYSTICKD_OPTS} \
    >"${LOG_DIR}/music-player-joystickd.log" 2>&1 &
  joystickd_pid=$!
  sleep 1
fi

export HOME="${STATE_DIR}"
export XDG_CONFIG_HOME="${STATE_DIR}/.config"
export PLUMOS_MALI_ROTATION="${PLUMOS_MALI_ROTATION:-auto}"
export PATH="${PLUMOS_ROOT}/bin:${PLUMOS_ROOT}/gnu/bin:/usr/sbin:/usr/bin:/sbin:/bin"

LIB_PATH="${PLUMOS_ROOT}/lib:${APP_ROOT}/lib:/usr/lib:/lib"
cd "${APP_ROOT}" || exit 1

"${LOADER}" \
  --library-path "${LIB_PATH}" \
  "${APP_ROOT}/bin/plumos-music-player.bin" \
  >"${LOG_DIR}/music-player.log" 2>&1
rc=$?
sync
exit "${rc}"
EOF
  chmod 0755 "${out}"
}

main() {
  local include_dir
  local app_dir="${TARGET_DIR}/plumos/apps/music-player"
  local bin_dir="${app_dir}/bin"
  local lib_dir="${app_dir}/lib"
  local root_lib_dir="${TARGET_DIR}/plumos/lib"
  local doc_dir="${TARGET_DIR}/plumos/share/doc/plumos-music-player"
  local log_dir="${TARGET_DIR}/docs/build-logs"
  local build_log="${log_dir}/music-player.log"
  local out_bin="${bin_dir}/plumos-music-player.bin"

  rm -rf "${TARGET_DIR}"
  mkdir -p "${bin_dir}" "${lib_dir}" "${root_lib_dir}" "${doc_dir}" "${log_dir}" \
    "${TARGET_DIR}/plumos/bin"

  include_dir=$(fetch_miniaudio)

  msg "building plumOS music player"
  {
    "$(tool_path "${CC}")" \
      -std=c11 -O2 -pipe -D_GNU_SOURCE \
      -DPLUMOS_ENABLE_MALI_RENDERER=1 \
      -DPLUMOS_ENABLE_MALI_FREETYPE=1 \
      -I"${include_dir}" \
      -I"${ROOT_DIR}/src/frontend" \
      -I/usr/include/freetype2 \
      -Wl,--dynamic-linker=/mnt/SDCARD/plumos/lib/ld-linux-armhf.so.3 \
      -o "${out_bin}" \
      "${ROOT_DIR}/src/apps/plumos_music_player.c" \
      -ldl -lfreetype -lm -lpthread
  } >"${build_log}" 2>&1

  "${STRIP}" "${out_bin}" 2>/dev/null || true
  copy_dep_tree "${out_bin}" "${root_lib_dir}"
  write_launcher "${TARGET_DIR}/plumos/bin/plumos-music-player-launch"

  {
    printf 'plumOS Music Player\n'
    printf 'binary=plumos/apps/music-player/bin/plumos-music-player.bin\n'
    printf 'launcher=plumos/bin/plumos-music-player-launch\n'
    printf 'music_roots=/mnt/SDCARD/Music,/mnt/SDCARD/Roms/music,/mnt/SDCARD/Roms/MUSIC\n'
    printf 'formats=mp3,flac,wav\n'
    printf 'audio_output=/dev/dsp OSS S16 stereo 44100Hz\n'
    printf 'decoder=%s\n' "${MINIAUDIO_REPO}"
    printf 'decoder_ref=%s\n' "${MINIAUDIO_REF}"
    printf 'controls=A play/pause; B or Function exit; Left/Right seek 5 seconds; X/Y track; Select EQ; L/R volume\n'
    printf '\n[needed]\n'
    "${READELF}" -d "${out_bin}" 2>/dev/null |
      awk -F"[][]" '/NEEDED/ {print $2}' || true
  } >"${doc_dir}/manifest.txt"
  cp -f "${build_log}" "${doc_dir}/build.log"

  msg "staged ${TARGET_DIR}"
}

main "$@"
