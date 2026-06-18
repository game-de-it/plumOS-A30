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
PICOARCH_A30_INPUT_PATCH=${PICOARCH_A30_INPUT_PATCH:-"${PATCH_DIR}/picoarch-a30-input.patch"}
PICOARCH_A30_DIRECT_PRESENT_PATCH=${PICOARCH_A30_DIRECT_PRESENT_PATCH:-"${PATCH_DIR}/picoarch-a30-direct-present.patch"}
PICOARCH_A30_PIXEL_FORMAT_PATCH=${PICOARCH_A30_PIXEL_FORMAT_PATCH:-"${PATCH_DIR}/picoarch-a30-pixel-format.patch"}
PICOARCH_A30_LIBRETRO_ENV_PATCH=${PICOARCH_A30_LIBRETRO_ENV_PATCH:-"${PATCH_DIR}/picoarch-a30-libretro-env.patch"}
PICOARCH_A30_CORE_OPTIONS_PATCH=${PICOARCH_A30_CORE_OPTIONS_PATCH:-"${PATCH_DIR}/picoarch-a30-core-options.patch"}
PICOARCH_A30_CONTENT_DIR_PATCH=${PICOARCH_A30_CONTENT_DIR_PATCH:-"${PATCH_DIR}/picoarch-a30-content-dir.patch"}
PICOARCH_A30_LOG_FLUSH_PATCH=${PICOARCH_A30_LOG_FLUSH_PATCH:-"${PATCH_DIR}/picoarch-a30-log-flush.patch"}
PICOARCH_A30_LIBRETRO_COMPAT_PATCH=${PICOARCH_A30_LIBRETRO_COMPAT_PATCH:-"${PATCH_DIR}/picoarch-a30-libretro-compat.patch"}
PICOARCH_A30_VFS_NORMALIZE_PATCH=${PICOARCH_A30_VFS_NORMALIZE_PATCH:-"${PATCH_DIR}/picoarch-a30-vfs-normalize.patch"}
PICOARCH_A30_KEYBOARD_INPUT_PATCH=${PICOARCH_A30_KEYBOARD_INPUT_PATCH:-"${PATCH_DIR}/picoarch-a30-keyboard-input.patch"}
PICOARCH_A30_FRAME_TIME_CALLBACK_PATCH=${PICOARCH_A30_FRAME_TIME_CALLBACK_PATCH:-"${PATCH_DIR}/picoarch-a30-frame-time-callback.patch"}
PICOARCH_A30_VIDEO_TRACE_PATCH=${PICOARCH_A30_VIDEO_TRACE_PATCH:-"${PATCH_DIR}/picoarch-a30-video-trace.patch"}

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
  git -C "${SRC_DIR}" clean -fdx
  rm -f "${SRC_DIR}/plat_a30_mali.c" "${SRC_DIR}/plat_a30_mali.h"
  git -C "${SRC_DIR}" submodule update --init --recursive --depth 1
  git -C "${SRC_DIR}" submodule foreach --recursive 'git reset --hard >/dev/null && git clean -fdx >/dev/null'
  rm -f "${SRC_DIR}/libpicofe/.patched"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_PATCH}"
  patch -d "${SRC_DIR}/libpicofe" -p1 <"${SRC_DIR}/patches/libpicofe/0001-key-combos.patch"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_INPUT_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_DIRECT_PRESENT_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_PIXEL_FORMAT_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_LIBRETRO_ENV_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_CORE_OPTIONS_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_CONTENT_DIR_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_LOG_FLUSH_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_LIBRETRO_COMPAT_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_VFS_NORMALIZE_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_KEYBOARD_INPUT_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_FRAME_TIME_CALLBACK_PATCH}"
  patch -d "${SRC_DIR}" -p1 <"${PICOARCH_A30_VIDEO_TRACE_PATCH}"
  if find "${SRC_DIR}" -name '*.rej' -print -quit | grep -q .; then
    msg "error: patch rejects remain under ${SRC_DIR}"
    find "${SRC_DIR}" -name '*.rej' -print >&2
    exit 1
  fi
  touch "${SRC_DIR}/libpicofe/.patched"
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
  PLUMOS_PICOARCH_JOYSTICKD_MODE keyboard or xbox. Default: xbox.
  PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE axisYL, axisXL, axisYR, or axisXR.
  PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE axisYL, axisXL, axisYR, or axisXR.
  PLUMOS_PICOARCH_JOYSTICKD_INVERT_X 1 inverts virtual gamepad X axis.
  PLUMOS_PICOARCH_JOYSTICKD_INVERT_Y 1 inverts virtual gamepad Y axis.
  PLUMOS_PICOARCH_SYSTEM       plumOS system id, used for system-specific core defaults.
  PLUMOS_PICOARCH_ROTATED_JOYSTICKD_X_SOURCE
                                Default X source for rotated-axis cores. Default: axisYR.
  PLUMOS_PICOARCH_ROTATED_JOYSTICKD_Y_SOURCE
                                Default Y source for rotated-axis cores. Default: axisXR.
  PLUMOS_PICOARCH_SCUMMVM_JOYSTICKD_X_SOURCE
                                ScummVM-only X source override. Default: axisYR.
  PLUMOS_PICOARCH_SCUMMVM_JOYSTICKD_Y_SOURCE
                                ScummVM-only Y source override. Default: axisXR.
  PLUMOS_PICOARCH_HATARI_JOYSTICKD_X_SOURCE, PLUMOS_PICOARCH_PRBOOM_JOYSTICKD_X_SOURCE,
  PLUMOS_PICOARCH_DOSBOX_PURE_JOYSTICKD_X_SOURCE
                                Per-core X source override for rotated-axis cores.
  PLUMOS_PICOARCH_HATARI_JOYSTICKD_Y_SOURCE, PLUMOS_PICOARCH_PRBOOM_JOYSTICKD_Y_SOURCE,
  PLUMOS_PICOARCH_DOSBOX_PURE_JOYSTICKD_Y_SOURCE
                                Per-core Y source override for rotated-axis cores.
  PLUMOS_PICOARCH_MENU_REPEAT_MS Menu repeat interval in ms. Default: 180.
  PLUMOS_PICOARCH_MENU_REPEAT_INITIAL_MS Initial repeat delay in ms. Default: 550.
  PLUMOS_PICOARCH_LOG            0 disables per-launch logs. Default: 1.
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
picoarch_pid=
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

find_resident_joystickd() {
  for f in /proc/[0-9]*/cmdline; do
    [ -e "$f" ] || continue
    pid=${f#/proc/}
    pid=${pid%/cmdline}
    [ "$pid" = "$$" ] && continue
    cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
    case "$cmd" in
      *"${PLUMOS_ROOT}/bin/plumos-joystickd"*)
        printf '%s\n' "$pid"
        ;;
    esac
  done
}

stop_resident_joystickd() {
  pids=$(find_resident_joystickd)
  [ -n "$pids" ] || return 0
  echo "stopping stale plumos-joystickd: $pids" >&2
  kill -TERM $pids >/dev/null 2>&1 || true
  sleep 1
  kill -KILL $pids >/dev/null 2>&1 || true
}

find_resident_picoarch() {
  for f in /proc/[0-9]*/cmdline; do
    [ -e "$f" ] || continue
    pid=${f#/proc/}
    pid=${pid%/cmdline}
    [ "$pid" = "$$" ] && continue
    [ -n "${picoarch_pid}" ] && [ "$pid" = "${picoarch_pid}" ] && continue
    cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
    case "$cmd" in
      *"${PICOARCH_BIN}"*)
        printf '%s\n' "$pid"
        ;;
    esac
  done
}

stop_resident_picoarch() {
  pids=$(find_resident_picoarch)
  [ -n "$pids" ] || return 0
  echo "stopping stale picoarch: $pids" >&2
  kill -TERM $pids >/dev/null 2>&1 || true
  sleep 1
  kill -KILL $pids >/dev/null 2>&1 || true
}

find_plumos_gamepad_js() {
  for js in /sys/class/input/js*; do
    [ -e "$js" ] || continue
    name=$(cat "$js/device/name" 2>/dev/null || true)
    [ "$name" = "plumOS A30 Gamepad" ] || continue
    printf '/dev/input/%s\n' "${js##*/}"
    return 0
  done
  return 1
}

cleanup() {
  rc=$?
  trap - EXIT HUP INT TERM
  if [ -n "${picoarch_pid}" ]; then
    kill -TERM "${picoarch_pid}" >/dev/null 2>&1 || true
    sleep 1
    kill -KILL "${picoarch_pid}" >/dev/null 2>&1 || true
    wait "${picoarch_pid}" 2>/dev/null || true
    picoarch_pid=
  fi
  stop_resident_picoarch
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

apply_rotated_joystickd_sources() {
  core_prefix=$1
  default_x=${PLUMOS_PICOARCH_ROTATED_JOYSTICKD_X_SOURCE:-axisYR}
  default_y=${PLUMOS_PICOARCH_ROTATED_JOYSTICKD_Y_SOURCE:-axisXR}
  eval core_x="\${PLUMOS_PICOARCH_${core_prefix}_JOYSTICKD_X_SOURCE:-}"
  eval core_y="\${PLUMOS_PICOARCH_${core_prefix}_JOYSTICKD_Y_SOURCE:-}"
  eval core_invert_x="\${PLUMOS_PICOARCH_${core_prefix}_JOYSTICKD_INVERT_X:-}"
  eval core_invert_y="\${PLUMOS_PICOARCH_${core_prefix}_JOYSTICKD_INVERT_Y:-}"

  PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE=${core_x:-${PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE:-${default_x}}}
  PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE=${core_y:-${PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE:-${default_y}}}
  PLUMOS_PICOARCH_JOYSTICKD_INVERT_X=${core_invert_x:-${PLUMOS_PICOARCH_JOYSTICKD_INVERT_X:-0}}
  PLUMOS_PICOARCH_JOYSTICKD_INVERT_Y=${core_invert_y:-${PLUMOS_PICOARCH_JOYSTICKD_INVERT_Y:-0}}
}

start_joystickd() {
  [ "${PLUMOS_PICOARCH_JOYSTICKD:-1}" != 0 ] || return 0
  [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ] || return 0
  stop_resident_joystickd

  joystickd_core_name=$(basename "${core_path}")
  joystickd_core_name=${joystickd_core_name%_libretro.so}
  joystickd_core_name=${joystickd_core_name%.so}
  case "${joystickd_core_name}" in
    scummvm)
      apply_rotated_joystickd_sources SCUMMVM
      ;;
    hatari)
      apply_rotated_joystickd_sources HATARI
      ;;
    prboom)
      apply_rotated_joystickd_sources PRBOOM
      ;;
    dosbox_pure)
      apply_rotated_joystickd_sources DOSBOX_PURE
      ;;
  esac

  joystickd_mode=${PLUMOS_PICOARCH_JOYSTICKD_MODE:-xbox}
  case "${joystickd_mode}" in
    xbox)
      joystickd_opts="--device-mode xbox --trigger-mode buttons --shoulder-layout user"
      ;;
    keyboard)
      joystickd_opts="--device-mode keyboard --keyboard-profile passthrough --trigger-mode buttons --shoulder-layout user"
      ;;
    *)
      echo "error: invalid joystickd mode: ${joystickd_mode}" >&2
      exit 2
      ;;
  esac

  case "${PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE:-}" in
    '') ;;
    axisYL|axisXL|axisYR|axisXR) joystickd_opts="${joystickd_opts} --x-source ${PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE}" ;;
    *) echo "warning: invalid PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE=${PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE}" >&2 ;;
  esac
  case "${PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE:-}" in
    '') ;;
    axisYL|axisXL|axisYR|axisXR) joystickd_opts="${joystickd_opts} --y-source ${PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE}" ;;
    *) echo "warning: invalid PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE=${PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE}" >&2 ;;
  esac
  case "${PLUMOS_PICOARCH_JOYSTICKD_INVERT_X:-0}" in
    1|yes|YES|true|TRUE) joystickd_opts="${joystickd_opts} --invert-x" ;;
  esac
  case "${PLUMOS_PICOARCH_JOYSTICKD_INVERT_Y:-0}" in
    1|yes|YES|true|TRUE) joystickd_opts="${joystickd_opts} --invert-y" ;;
  esac

  "${PLUMOS_ROOT}/bin/plumos-joystickd" ${joystickd_opts} \
    >"${LOG_DIR}/joystickd-last.log" 2>&1 &
  joystickd_pid=$!
  sleep 1
  gamepad_js=$(find_plumos_gamepad_js || true)
  if [ -n "${gamepad_js}" ]; then
    SDL_JOYSTICK_DEVICE=${gamepad_js}
    export SDL_JOYSTICK_DEVICE
  fi
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

mkdir -p "${STATE_DIR}" "${LOG_DIR}" \
  "${STATE_DIR}/xdg-config" "${STATE_DIR}/xdg-data" "${STATE_DIR}/xdg-cache"
trap cleanup EXIT HUP INT TERM

stop_resident_picoarch
restore_framebuffer
apply_cpu_policy "${PLUMOS_PICOARCH_CPU_POLICY:-keep}" \
  "${PLUMOS_PICOARCH_CPU_FREQ:-648000}" \
  "${PLUMOS_PICOARCH_CPU_CORES:-keep}"
start_joystickd

core_log_name=$(basename "${core_path}")
core_log_name=${core_log_name%_libretro.so}
core_log_name=${core_log_name%.so}
core_log_name=$(printf '%s' "${core_log_name}" | tr -c 'A-Za-z0-9_.-' '_')

export HOME=${STATE_DIR}
export XDG_CONFIG_HOME=${STATE_DIR}/xdg-config
export XDG_DATA_HOME=${STATE_DIR}/xdg-data
export XDG_CACHE_HOME=${STATE_DIR}/xdg-cache
export SDCARD_PATH=${SDCARD_ROOT}
case "${core_log_name}" in
  chailove)
    export SDL_VIDEODRIVER=LIBRETROvideo
    ;;
  *)
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-fbcon}
    ;;
esac
export SDL_FBDEV=${SDL_FBDEV:-/dev/fb0}
export SDL_NOMOUSE=${SDL_NOMOUSE:-1}
export SDL_AUDIODRIVER=${PLUMOS_PICOARCH_SDL_AUDIODRIVER:-${SDL_AUDIODRIVER:-alsa}}
export AUDIODEV=${AUDIODEV:-default}
export PLUMOS_PICOARCH_A30_MALI=${PLUMOS_PICOARCH_A30_MALI:-1}
export PLUMOS_PICOARCH_A30_ROTATION=${PLUMOS_PICOARCH_A30_ROTATION:-ccw}
export PLUMOS_PICOARCH_A30_VSYNC=${PLUMOS_PICOARCH_A30_VSYNC:-1}
export PLUMOS_PICOARCH_A30_LINEAR=${PLUMOS_PICOARCH_A30_LINEAR:-0}
export PLUMOS_PICOARCH_LIBRARY_PATH="${PLUMOS_PICOARCH_LIBRARY_PATH:-${PICOARCH_LIB}:${PLUMOS_LIB}:/mnt/SDCARD/miyoo/lib:/usr/lib:/lib}"

run_picoarch() {
  exec "${LOADER}" \
    --library-path "${PLUMOS_PICOARCH_LIBRARY_PATH}" \
    "${PICOARCH_BIN}" "${core_path}" "${rom_path}" "${scale_effect}"
}

picoarch_tag_name() {
  path=$1
  roms_prefix=${SDCARD_ROOT}/Roms/
  case "${path}" in
    "${roms_prefix}"*)
      rel=${path#"${roms_prefix}"}
      tag=${rel%%/*}
      ;;
    *)
      tag=$(basename "${path}")
      tag=${tag%.*}
      ;;
  esac
  case "${tag}" in
    *"("*")"*)
      inner=${tag#*(}
      tag=${inner%%)*}
      ;;
  esac
  printf '%s\n' "${tag}"
}

set_picoarch_config_value() {
  config_path=$1
  key=$2
  value=$3
  tmp_path=${config_path}.tmp.$$

  if grep -q "^${key} = " "${config_path}" 2>/dev/null; then
    sed "s|^${key} = .*|${key} = ${value}|" "${config_path}" >"${tmp_path}" &&
      mv "${tmp_path}" "${config_path}" || {
        rm -f "${tmp_path}"
        return 1
      }
  else
    printf '%s = %s\n' "${key}" "${value}" >>"${config_path}"
  fi
}

seed_picoarch_system_core_options() {
  config_path=$1

  case "${core_log_name}" in
    ecwolf)
      set_picoarch_config_value "${config_path}" ecwolf-palette xrgb8888
      return 0
      ;;
    atari800) ;;
    *) return 0 ;;
  esac

  case "${PLUMOS_PICOARCH_SYSTEM:-}" in
    atari5200)
      set_picoarch_config_value "${config_path}" atari800_artifacting disabled
      set_picoarch_config_value "${config_path}" atari800_cassboot disabled
      set_picoarch_config_value "${config_path}" atari800_internalbasic disabled
      set_picoarch_config_value "${config_path}" atari800_keyboard poll
      set_picoarch_config_value "${config_path}" atari800_ntscpal NTSC
      set_picoarch_config_value "${config_path}" atari800_opt1 enabled
      set_picoarch_config_value "${config_path}" atari800_opt2 disabled
      set_picoarch_config_value "${config_path}" atari800_resolution 336x240
      set_picoarch_config_value "${config_path}" atari800_sioaccel enabled
      set_picoarch_config_value "${config_path}" atari800_system 5200
      ;;
    atari800)
      set_picoarch_config_value "${config_path}" atari800_artifacting disabled
      set_picoarch_config_value "${config_path}" atari800_cassboot disabled
      set_picoarch_config_value "${config_path}" atari800_internalbasic disabled
      set_picoarch_config_value "${config_path}" atari800_keyboard poll
      set_picoarch_config_value "${config_path}" atari800_ntscpal PAL
      set_picoarch_config_value "${config_path}" atari800_opt1 disabled
      set_picoarch_config_value "${config_path}" atari800_opt2 disabled
      set_picoarch_config_value "${config_path}" atari800_resolution 336x240
      set_picoarch_config_value "${config_path}" atari800_sioaccel enabled
      set_picoarch_config_value "${config_path}" atari800_system "Modern XL/XE(1088K)"
      ;;
    *)
      return 0
      ;;
  esac

  if [ -n "${PLUMOS_PICOARCH_BIOS_DIR:-}" ]; then
    set_picoarch_config_value "${config_path}" bios_dir "${PLUMOS_PICOARCH_BIOS_DIR}"
  fi
}

seed_picoarch_input_config() {
  tag_name=$(picoarch_tag_name "${rom_path}")
  [ -n "${tag_name}" ] || return 0

  config_dir=${STATE_DIR}/.picoarch-${core_log_name}-${tag_name}
  config_path=${config_dir}/picoarch.cfg
  mkdir -p "${config_dir}" 2>/dev/null || return 0

  if [ ! -s "${config_path}" ]; then
    cat >"${config_path}" <<'CFG'
binddev = sdl:keys
bind backspace = player1 R BUTTON
bind tab = player1 L BUTTON
bind return = player1 START
bind escape = menu
bind space = player1 A BUTTON
bind \ = player1 R2 BUTTON
bind q = player1 L2 BUTTON
bind \xA0 = player1 A BUTTON
bind \xA1 = player1 B BUTTON
bind \xA2 = player1 X BUTTON
bind \xA3 = player1 Y BUTTON
bind \xA4 = player1 L BUTTON
bind \xA5 = player1 R BUTTON
bind \xA6 = player1 L2 BUTTON
bind \xA7 = player1 R2 BUTTON
bind \xA8 = player1 SELECT
bind \xA9 = player1 START
bind \xAA = menu
bind up = player1 UP
bind down = player1 DOWN
bind right = player1 RIGHT
bind left = player1 LEFT
bind left shift = player1 X BUTTON
bind right ctrl = player1 SELECT
bind left ctrl = player1 B BUTTON
bind left alt = player1 Y BUTTON
binddev = sdl:plumOS A30 Gamepad
bind backspace = player1 R BUTTON
bind tab = player1 L BUTTON
bind return = player1 START
bind escape = menu
bind space = player1 A BUTTON
bind \ = player1 R2 BUTTON
bind q = player1 L2 BUTTON
bind \xA0 = player1 A BUTTON
bind \xA1 = player1 B BUTTON
bind \xA2 = player1 X BUTTON
bind \xA3 = player1 Y BUTTON
bind \xA4 = player1 L BUTTON
bind \xA5 = player1 R BUTTON
bind \xA6 = player1 L2 BUTTON
bind \xA7 = player1 R2 BUTTON
bind \xA8 = player1 SELECT
bind \xA9 = player1 START
bind \xAA = menu
bind up = player1 UP
bind down = player1 DOWN
bind right = player1 RIGHT
bind left = player1 LEFT
bind left shift = player1 X BUTTON
bind right ctrl = player1 SELECT
bind left ctrl = player1 B BUTTON
bind left alt = player1 Y BUTTON
CFG
  elif ! grep -q 'bind \\xAA = menu' "${config_path}" 2>/dev/null; then
    cat >>"${config_path}" <<'CFG'

binddev = sdl:keys
bind escape = menu
bind \xAA = menu
binddev = sdl:plumOS A30 Gamepad
bind escape = menu
bind \xAA = menu
CFG
  fi

  seed_picoarch_system_core_options "${config_path}"
}

uses_shared_bios_root() {
  case "$1" in
    scummvm|fceumm|nestopia|mednafen_pce_fast|mednafen_supergrafx|mednafen_pce|genesis_plus_gx|picodrive|pcsx_rearmed|mednafen_pcfx|opera|neocd)
      return 0
      ;;
    gpsp|mgba|mednafen_gba|meteor|vba_next|vbam|gambatte|gearboy|gearsystem|mednafen_lynx|handy|mednafen_ngp|mednafen_wswan|pokemini|freechaf|snes9x)
      return 0
      ;;
    atari800|prosystem|freeintv|o2em|bluemsx|fmsx|puae|np2kai|nekop2|px68k|hatari|cap32|x1|bk|mu|vice_x64|vice_xvic|fuse)
      return 0
      ;;
    flycast|yabasanshiro|beetle_saturn|yabause|mupen64plus_next|parallel_n64|virtualjaguar)
      return 0
      ;;
    fbneo|fbalpha2012|fbalpha2012_cps1|fbalpha2012_cps2|fbalpha2012_neogeo|mame2000|mame2003_plus)
      return 0
      ;;
    squirreljme|ecwolf|dosbox_pure|prboom)
      return 0
      ;;
  esac
  return 1
}

freechaf_bios_ready() {
  bios_dir=${PLUMOS_PICOARCH_BIOS_DIR:-${SDCARD_ROOT}/Bios}
  [ -r "${bios_dir}/sl31254.bin" ] || return 1
  [ -r "${bios_dir}/sl31253.bin" ] || [ -r "${bios_dir}/sl90025.bin" ] || return 1
  return 0
}

freechaf_bios_error() {
  bios_dir=${PLUMOS_PICOARCH_BIOS_DIR:-${SDCARD_ROOT}/Bios}
  printf 'error: FreeChaF requires BIOS in %s: sl31254.bin and one of sl31253.bin or sl90025.bin\n' "${bios_dir}"
  printf 'expected md5: sl31254=da98f4bb3242ab80d76629021bb27585, sl31253=ac9804d4c0e9d07e33472e3726ed15c3, sl90025=95d339631d867c8f1d15a5f2ec26069d\n'
}

if [ -z "${PLUMOS_PICOARCH_BIOS_DIR:-}" ]; then
  case "${core_log_name}" in
    quasi88)
      PLUMOS_PICOARCH_BIOS_DIR=${SDCARD_ROOT}/Bios/quasi88
      ;;
    *)
      if uses_shared_bios_root "${core_log_name}"; then
        # BIOS packs are staged in the shared Miyoo/Onion-style BIOS root.
        PLUMOS_PICOARCH_BIOS_DIR=${SDCARD_ROOT}/Bios
      fi
      ;;
  esac
fi
if [ -n "${PLUMOS_PICOARCH_BIOS_DIR:-}" ]; then
  export PLUMOS_PICOARCH_BIOS_DIR
else
  unset PLUMOS_PICOARCH_BIOS_DIR
fi

seed_picoarch_input_config

run_log=${LOG_DIR}/${core_log_name}-last.log
latest_log=${LOG_DIR}/last.log

if [ "${PLUMOS_PICOARCH_LOG:-1}" = 0 ]; then
  if [ "${core_log_name}" = freechaf ] && ! freechaf_bios_ready; then
    freechaf_bios_error >&2
    exit 66
  fi
  run_picoarch &
  picoarch_pid=$!
  wait "${picoarch_pid}"
  rc=$?
  picoarch_pid=
  exit "$rc"
fi

{
  printf 'timestamp=%s\n' "$(date '+%Y-%m-%dT%H:%M:%S%z' 2>/dev/null || true)"
  printf 'core=%s\n' "${core_path}"
  printf 'rom=%s\n' "${rom_path}"
  printf 'effect=%s\n' "${scale_effect}"
  printf 'cpu_policy=%s\n' "${PLUMOS_PICOARCH_CPU_POLICY:-keep}"
  printf 'cpu_freq=%s\n' "${PLUMOS_PICOARCH_CPU_FREQ:-648000}"
  printf 'cpu_cores=%s\n' "${PLUMOS_PICOARCH_CPU_CORES:-keep}"
  printf 'joystickd_mode=%s\n' "${PLUMOS_PICOARCH_JOYSTICKD_MODE:-xbox}"
  printf 'joystickd_x_source=%s\n' "${PLUMOS_PICOARCH_JOYSTICKD_X_SOURCE:-}"
  printf 'joystickd_y_source=%s\n' "${PLUMOS_PICOARCH_JOYSTICKD_Y_SOURCE:-}"
  printf 'joystickd_invert_x=%s\n' "${PLUMOS_PICOARCH_JOYSTICKD_INVERT_X:-0}"
  printf 'joystickd_invert_y=%s\n' "${PLUMOS_PICOARCH_JOYSTICKD_INVERT_Y:-0}"
  printf 'a30_mali=%s\n' "${PLUMOS_PICOARCH_A30_MALI}"
  printf 'a30_rotation=%s\n' "${PLUMOS_PICOARCH_A30_ROTATION}"
  printf 'a30_vsync=%s\n' "${PLUMOS_PICOARCH_A30_VSYNC}"
  printf 'a30_linear=%s\n' "${PLUMOS_PICOARCH_A30_LINEAR}"
  printf 'bios_dir=%s\n' "${PLUMOS_PICOARCH_BIOS_DIR:-}"
  printf '%s\n' '---'
} >"${run_log}"

if [ "${core_log_name}" = freechaf ] && ! freechaf_bios_ready; then
  freechaf_bios_error >&2
  freechaf_bios_error >>"${run_log}"
  cp "${run_log}" "${latest_log}" 2>/dev/null || true
  exit 66
fi

run_picoarch >>"${run_log}" 2>&1 &
picoarch_pid=$!
wait "${picoarch_pid}"
rc=$?
picoarch_pid=
cp "${run_log}" "${latest_log}" 2>/dev/null || true
exit "$rc"
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
#
# ScummVM libretro reads the analog cursor through the gamepad axes. On A30 the
# PicoArch launcher swaps X/Y for that core by default; override these only if a
# future joystickd mapping changes.
# PLUMOS_PICOARCH_SCUMMVM_JOYSTICKD_X_SOURCE=axisYR
# PLUMOS_PICOARCH_SCUMMVM_JOYSTICKD_Y_SOURCE=axisXR
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
    echo "a30_input_patch=$(basename "${PICOARCH_A30_INPUT_PATCH}")"
    echo "a30_direct_present_patch=$(basename "${PICOARCH_A30_DIRECT_PRESENT_PATCH}")"
    echo "a30_pixel_format_patch=$(basename "${PICOARCH_A30_PIXEL_FORMAT_PATCH}")"
    echo "a30_libretro_env_patch=$(basename "${PICOARCH_A30_LIBRETRO_ENV_PATCH}")"
    echo "a30_core_options_patch=$(basename "${PICOARCH_A30_CORE_OPTIONS_PATCH}")"
    echo "a30_content_dir_patch=$(basename "${PICOARCH_A30_CONTENT_DIR_PATCH}")"
    echo "a30_log_flush_patch=$(basename "${PICOARCH_A30_LOG_FLUSH_PATCH}")"
    echo "a30_libretro_compat_patch=$(basename "${PICOARCH_A30_LIBRETRO_COMPAT_PATCH}")"
    echo "a30_keyboard_input_patch=$(basename "${PICOARCH_A30_KEYBOARD_INPUT_PATCH}")"
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
