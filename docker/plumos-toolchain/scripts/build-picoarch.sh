#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-picoarch"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/picoarch"}
SRC_DIR=${SRC_DIR:-"${SRC_ROOT}/src"}
PATCH_DIR=${PATCH_DIR:-"${ROOT_DIR}/docker/plumos-toolchain/patches"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

PICOARCH_REPO=${PICOARCH_REPO:-https://github.com/shauninman/picoarch.git}
PICOARCH_REF=${PICOARCH_REF:-802047c276a5a931b0bf837c4ea4b8e238bdeabe}
PICOARCH_PATCH=${PICOARCH_PATCH:-"${PATCH_DIR}/picoarch-802047c-plumos-platform.patch"}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

BIN_DIR="${TARGET_DIR}/plumos/bin"
LOADER_DIR="${TARGET_DIR}/plumos/lib"
PICOARCH_BIN_DIR="${TARGET_DIR}/plumos/emulators/picoarch/bin"
PICOARCH_LIB_DIR="${TARGET_DIR}/plumos/emulators/picoarch/lib"
PICOARCH_CONFIG_DIR="${TARGET_DIR}/plumos/config/standalone"
DOC_DIR="${TARGET_DIR}/plumos/share/doc/picoarch"
MANIFEST="${DOC_DIR}/manifest.txt"

msg() {
  printf '[picoarch] %s\n' "$*" >&2
}

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

copy_runtime_deps() {
  local binary=$1
  local lib_dir=$2
  local lib path

  mkdir -p "${lib_dir}"
  for lib in ld-linux-armhf.so.3 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 \
    librt.so.1 libgcc_s.so.1 libstdc++.so.6; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}" "${lib}"
    fi
  done
  copy_dep_tree "${binary}" "${lib_dir}"
  chmod 0755 "${lib_dir}/ld-linux-armhf.so.3" 2>/dev/null || true
}

checkout_source() {
  mkdir -p "${SRC_ROOT}"
  if [ ! -d "${SRC_DIR}/.git" ]; then
    rm -rf "${SRC_DIR}"
    git clone "${PICOARCH_REPO}" "${SRC_DIR}"
  fi
  git -C "${SRC_DIR}" fetch --tags --force origin
  git -C "${SRC_DIR}" reset --hard "${PICOARCH_REF}"
  rm -f "${SRC_DIR}/plat_a30_mali.c" "${SRC_DIR}/plat_a30_mali.h"
  git -C "${SRC_DIR}" submodule update --init --recursive --depth 1
  git -C "${SRC_DIR}" submodule foreach --recursive 'git reset --hard >/dev/null && git clean -fdx >/dev/null'
  rm -f "${SRC_DIR}/libpicofe/.patched"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_PATCH}"
}

build_picoarch() {
  msg "building picoarch ${PICOARCH_REF}"
  make -C "${SRC_DIR}" clean >/dev/null 2>&1 || true
  make -C "${SRC_DIR}" \
    platform=plumos \
    MMENU=0 \
    CROSS_COMPILE="${CROSS_PREFIX}" \
    PROCS="-j${JOBS}" \
    picoarch
  "${STRIP}" "${SRC_DIR}/picoarch" 2>/dev/null || true
}

write_launcher() {
  mkdir -p "${BIN_DIR}"
  cat >"${BIN_DIR}/plumos-picoarch-launch" <<'EOF'
#!/bin/sh
set -u

usage() {
  cat <<'USAGE'
Usage: plumos-picoarch-launch CORE_ID_OR_SO ROM [NONE|DMG|LCD|SCANLINE]

CORE_ID_OR_SO is either a core id such as fceumm or a full *_libretro.so path.

Environment:
  PLUMOS_ROOT                    Default: /mnt/SDCARD/plumos
  PLUMOS_SDCARD_ROOT             Default: /mnt/SDCARD
  PLUMOS_PICOARCH_CPU_POLICY     keep, performance, fixed. Default: keep.
  PLUMOS_PICOARCH_CPU_FREQ       Fixed CPU frequency in kHz. Default: 648000.
  PLUMOS_PICOARCH_CPU_CORES      keep, 2, 4. Default: keep.
  PLUMOS_PICOARCH_JOYSTICKD      0 disables per-launch joystickd. Default: 1.
  PLUMOS_PICOARCH_SDL_AUDIODRIVER Default: alsa.
  PLUMOS_PICOARCH_BIOS_DIR       Default BIOS/system dir override for libretro cores.
  PLUMOS_PICOARCH_A30_MALI       0 disables the A30 Mali presenter. Default: 1.
  PLUMOS_PICOARCH_A30_ROTATION   cw, ccw, 180, none. Default: ccw.
  PLUMOS_PICOARCH_A30_VSYNC      0 disables EGL swap interval. Default: 1.
  PLUMOS_PICOARCH_A30_LINEAR     1 enables GL_LINEAR texture filtering. Default: 0.
  PLUMOS_PICOARCH_A30_FALLBACK_SDL
                                1 allows stock SDL video fallback if presenter init fails. Default: 0.
USAGE
}

if [ "$#" -lt 2 ] || [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

core_arg=$1
rom_path=$2
scale_effect=${3:-NONE}

case "${scale_effect}" in
  NONE|DMG|LCD|SCANLINE) ;;
  *) echo "error: invalid scale effect: ${scale_effect}" >&2; exit 2 ;;
esac

PLUMOS_ROOT=${PLUMOS_ROOT:-/mnt/SDCARD/plumos}
SDCARD_ROOT=${PLUMOS_SDCARD_ROOT:-/mnt/SDCARD}
CONFIG_DIR=${PLUMOS_ROOT}/config/standalone
PLUMOS_LIB=${PLUMOS_ROOT}/lib
PICOARCH_LIB=${PLUMOS_ROOT}/emulators/picoarch/lib
LOADER=${PLUMOS_LIB}/ld-linux-armhf.so.3
PICOARCH_BIN=${PLUMOS_ROOT}/emulators/picoarch/bin/picoarch
STATE_DIR=${PLUMOS_ROOT}/state/picoarch
LOG_DIR=${PLUMOS_ROOT}/logs/picoarch
CPU_STATE=/tmp/plumos-picoarch-cpustate-$$
joystickd_pid=
cpu_policy_applied=0

load_env_file() {
  env_file=$1
  [ -r "${env_file}" ] || return 0
  set -a
  . "${env_file}"
  set +a
}

load_env_file "${CONFIG_DIR}/picoarch.env"

case "${core_arg}" in
  */*|*.so)
    core_path=${core_arg}
    ;;
  *)
    core_path=${PLUMOS_ROOT}/retroarch/cores/${core_arg}_libretro.so
    ;;
esac

restore_framebuffer() {
  if [ -x "${PLUMOS_ROOT}/bin/plumos-fb-restore" ]; then
    "${PLUMOS_ROOT}/bin/plumos-fb-restore" --quiet >/dev/null 2>&1 || true
  fi
}

cleanup() {
  rc=$?
  trap - EXIT HUP INT TERM
  if [ -n "${joystickd_pid}" ]; then
    kill -TERM "${joystickd_pid}" >/dev/null 2>&1 || true
    sleep 1
    kill -KILL "${joystickd_pid}" >/dev/null 2>&1 || true
  fi
  restore_cpu_policy
  restore_framebuffer
  exit "$rc"
}

cpu_freq_dir() {
  printf '%s\n' /sys/devices/system/cpu/cpu0/cpufreq
}

save_cpu_policy() {
  p=$(cpu_freq_dir)
  {
    printf "governor=%s\n" "$(cat "$p/scaling_governor" 2>/dev/null || true)"
    printf "min_freq=%s\n" "$(cat "$p/scaling_min_freq" 2>/dev/null || true)"
    printf "max_freq=%s\n" "$(cat "$p/scaling_max_freq" 2>/dev/null || true)"
    for cpu in 1 2 3; do
      if [ -r "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
        printf "cpu%s_online=%s\n" "$cpu" "$(cat "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true)"
      fi
    done
  } >"${CPU_STATE}"
}

restore_cpu_policy() {
  [ "${cpu_policy_applied}" = 1 ] || return 0
  [ -f "${CPU_STATE}" ] || return 0
  . "${CPU_STATE}" 2>/dev/null || true
  for cpu in 1 2 3; do
    eval value="\${cpu${cpu}_online:-}"
    if [ -n "${value}" ] && [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
      echo "${value}" >"/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
    fi
  done
  p=$(cpu_freq_dir)
  [ -n "${governor:-}" ] && echo "${governor}" >"$p/scaling_governor" 2>/dev/null || true
  [ -n "${min_freq:-}" ] && echo "${min_freq}" >"$p/scaling_min_freq" 2>/dev/null || true
  [ -n "${max_freq:-}" ] && echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
  rm -f "${CPU_STATE}"
  cpu_policy_applied=0
}

apply_cpu_policy() {
  policy=$1
  freq=$2
  cores=$3

  case "${policy}" in
    keep) ;;
    performance|fixed) ;;
    *) echo "error: invalid CPU policy: ${policy}" >&2; exit 2 ;;
  esac
  case "${cores}" in
    keep|2|4) ;;
    *) echo "error: invalid CPU cores: ${cores}" >&2; exit 2 ;;
  esac
  [ "${policy}" != keep ] || [ "${cores}" != keep ] || return 0

  save_cpu_policy
  cpu_policy_applied=1

  case "${cores}" in
    2)
      [ -w /sys/devices/system/cpu/cpu1/online ] && echo 1 >/sys/devices/system/cpu/cpu1/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu2/online ] && echo 0 >/sys/devices/system/cpu/cpu2/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu3/online ] && echo 0 >/sys/devices/system/cpu/cpu3/online 2>/dev/null || true
      ;;
    4)
      for cpu in 1 2 3; do
        [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ] && echo 1 >"/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
      done
      ;;
  esac

  p=$(cpu_freq_dir)
  case "${policy}" in
    performance)
      max_freq=$(cat "$p/cpuinfo_max_freq" 2>/dev/null || echo "${freq}")
      echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      echo performance >"$p/scaling_governor" 2>/dev/null || true
      ;;
    fixed)
      case "${freq}" in ''|*[!0-9]*) echo "error: invalid CPU frequency: ${freq}" >&2; exit 2 ;; esac
      echo "${freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      echo "${freq}" >"$p/scaling_min_freq" 2>/dev/null || true
      echo userspace >"$p/scaling_governor" 2>/dev/null || true
      [ -w "$p/scaling_setspeed" ] && echo "${freq}" >"$p/scaling_setspeed" 2>/dev/null || true
      ;;
  esac
}

start_joystickd() {
  [ "${PLUMOS_PICOARCH_JOYSTICKD:-1}" != 0 ] || return 0
  [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ] || return 0
  "${PLUMOS_ROOT}/bin/plumos-joystickd" \
    --device-mode keyboard \
    --keyboard-profile passthrough \
    --trigger-mode buttons \
    --shoulder-layout user \
    >"${LOG_DIR}/joystickd-last.log" 2>&1 &
  joystickd_pid=$!
  sleep 1
}

if [ ! -x "${LOADER}" ]; then
  echo "error: missing loader: ${LOADER}" >&2
  exit 127
fi
if [ ! -x "${PICOARCH_BIN}" ]; then
  echo "error: missing picoarch binary: ${PICOARCH_BIN}" >&2
  exit 127
fi
if [ ! -r "${core_path}" ]; then
  echo "error: missing libretro core: ${core_path}" >&2
  exit 66
fi
if [ ! -e "${rom_path}" ]; then
  echo "error: missing ROM: ${rom_path}" >&2
  exit 66
fi

mkdir -p "${STATE_DIR}" "${LOG_DIR}"
trap cleanup EXIT HUP INT TERM

restore_framebuffer
apply_cpu_policy "${PLUMOS_PICOARCH_CPU_POLICY:-keep}" \
  "${PLUMOS_PICOARCH_CPU_FREQ:-648000}" \
  "${PLUMOS_PICOARCH_CPU_CORES:-keep}"
start_joystickd

export HOME=${STATE_DIR}
export SDCARD_PATH=${SDCARD_ROOT}
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-fbcon}
export SDL_FBDEV=${SDL_FBDEV:-/dev/fb0}
export SDL_NOMOUSE=${SDL_NOMOUSE:-1}
export SDL_AUDIODRIVER=${PLUMOS_PICOARCH_SDL_AUDIODRIVER:-${SDL_AUDIODRIVER:-alsa}}
export AUDIODEV=${AUDIODEV:-default}
export PLUMOS_PICOARCH_BIOS_DIR=${PLUMOS_PICOARCH_BIOS_DIR:-}
export PLUMOS_PICOARCH_A30_MALI=${PLUMOS_PICOARCH_A30_MALI:-1}
export PLUMOS_PICOARCH_A30_ROTATION=${PLUMOS_PICOARCH_A30_ROTATION:-ccw}
export PLUMOS_PICOARCH_A30_VSYNC=${PLUMOS_PICOARCH_A30_VSYNC:-1}
export PLUMOS_PICOARCH_A30_LINEAR=${PLUMOS_PICOARCH_A30_LINEAR:-0}

"${LOADER}" \
  --library-path "${PICOARCH_LIB}:/usr/lib:/lib:/mnt/SDCARD/miyoo/lib:${PLUMOS_LIB}" \
  "${PICOARCH_BIN}" "${core_path}" "${rom_path}" "${scale_effect}"
exit $?
EOF
  chmod 0755 "${BIN_DIR}/plumos-picoarch-launch"
}

prepare_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p "${PICOARCH_BIN_DIR}" "${DOC_DIR}" "${LOADER_DIR}" "${PICOARCH_LIB_DIR}" \
    "${PICOARCH_CONFIG_DIR}"
}

write_default_config() {
  cat >"${PICOARCH_CONFIG_DIR}/picoarch.env" <<'EOF'
# plumOS PicoArch launch overrides.
# Leave empty to use PicoArch's per-system default: /mnt/SDCARD/Bios/<ROM_DIR>.
# Example:
# PLUMOS_PICOARCH_BIOS_DIR=/mnt/SDCARD/Bios
EOF
}

stage_outputs() {
  local loader_path

  install -m 0755 "${SRC_DIR}/picoarch" "${PICOARCH_BIN_DIR}/picoarch"
  write_launcher
  copy_runtime_deps "${PICOARCH_BIN_DIR}/picoarch" "${PICOARCH_LIB_DIR}"
  loader_path=$(find_target_lib ld-linux-armhf.so.3 || true)
  if [ -n "${loader_path}" ]; then
    copy_if_present "${loader_path}" "${LOADER_DIR}" "ld-linux-armhf.so.3"
    chmod 0755 "${LOADER_DIR}/ld-linux-armhf.so.3" 2>/dev/null || true
  fi
  write_default_config
  cp -f "${SRC_DIR}/README.md" "${DOC_DIR}/README.md" 2>/dev/null || true
  cp -f "${SRC_DIR}/LICENSE" "${DOC_DIR}/LICENSE" 2>/dev/null || true
  {
    echo "plumOS picoarch build"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "repo=${PICOARCH_REPO}"
    echo "ref=${PICOARCH_REF}"
    echo "patch=$(basename "${PICOARCH_PATCH}")"
    echo "a30_mali_presenter=enabled"
    echo "a30_mali_default_rotation=ccw"
    echo "a30_mali_default_vsync=1"
    echo "a30_mali_default_filter=nearest"
    echo "config=plumos/config/standalone/picoarch.env"
    echo "cc=$("${CC}" --version | head -n 1)"
    echo "target_dir=${TARGET_DIR}"
    echo
    echo "binary:"
    "${READELF}" -d "${PICOARCH_BIN_DIR}/picoarch" | grep NEEDED || true
    echo
    sha256sum "${PICOARCH_BIN_DIR}/picoarch" "${BIN_DIR}/plumos-picoarch-launch"
  } >"${MANIFEST}"
}

checkout_source
build_picoarch
prepare_dist
stage_outputs
msg "wrote ${TARGET_DIR}"
