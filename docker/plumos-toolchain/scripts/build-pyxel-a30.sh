#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PYXEL_VERSION="${PYXEL_VERSION:-2.9.6}"
PYXEL_REF="${PYXEL_REF:-v${PYXEL_VERSION}}"
PYXEL_REPO="${PYXEL_REPO:-https://github.com/kitao/pyxel.git}"
PYXEL_PATCH="${PYXEL_PATCH:-${ROOT_DIR}/docker/plumos-toolchain/patches/pyxel-2.9.6-a30-fbo-present.patch}"
RUST_TOOLCHAIN="${RUST_TOOLCHAIN:-nightly-2026-06-12}"
TARGET_TRIPLE="${TARGET_TRIPLE:-armv7-unknown-linux-gnueabihf}"

BUILD_ROOT="${ROOT_DIR}/build/pyxel-a30"
SRC_DIR="${BUILD_ROOT}/pyxel-${PYXEL_VERSION}"
WHEEL_DIR="${BUILD_ROOT}/wheel"
PYTHON_USER_BASE="${BUILD_ROOT}/python-user"
DIST_DIR="${ROOT_DIR}/dist/plumos-pyxel-a30"
SITE_DIR="${DIST_DIR}/plumos/experiments/pyxel-a30-site"
BIN_DIR="${DIST_DIR}/plumos/bin"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-pyxel-a30"
WHEEL_COPY_DIR="${DIST_DIR}/plumos/share/pyxel-a30"
SHIM_SOURCE="${ROOT_DIR}/docker/plumos-toolchain/pyxel-a30-shim/plumos_pyxel_a30_shim.py"

log() {
  printf '%s\n' "==> $*"
}

die() {
  printf '%s\n' "error: $*" >&2
  exit 1
}

ensure_rustup() {
  export CARGO_HOME="${CARGO_HOME:-${ROOT_DIR}/build/cargo-home}"
  export RUSTUP_HOME="${RUSTUP_HOME:-${ROOT_DIR}/build/rustup}"
  export PATH="${CARGO_HOME}/bin:${PATH}"

  if ! command -v rustup >/dev/null 2>&1; then
    log "Installing Rust toolchain ${RUST_TOOLCHAIN}"
    curl -fsSL https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain "$RUST_TOOLCHAIN"
  fi

  rustup toolchain install "$RUST_TOOLCHAIN" --profile minimal
  rustup target add "$TARGET_TRIPLE" --toolchain "$RUST_TOOLCHAIN"
}

prepare_source() {
  rm -rf "$SRC_DIR"
  mkdir -p "$BUILD_ROOT"
  log "Cloning Pyxel ${PYXEL_REF}"
  git clone --depth 1 --branch "$PYXEL_REF" "$PYXEL_REPO" "$SRC_DIR"

  log "Applying A30 present patch"
  patch -d "$SRC_DIR" -p1 < "$PYXEL_PATCH"
  cp "$SRC_DIR/LICENSE" "$SRC_DIR/python/pyxel/"
  cp "$SRC_DIR/README.md" "$SRC_DIR/python/pyxel/"
}

build_wheel() {
  log "Installing maturin"
  PYTHONUSERBASE="$PYTHON_USER_BASE" python3 -m pip install --user -q --break-system-packages 'maturin>=1.0,<2.0'
  export PATH="${PYTHON_USER_BASE}/bin:${PATH}"

  rm -rf "$WHEEL_DIR"
  mkdir -p "$WHEEL_DIR"

  (
    cd "$SRC_DIR/python"
    export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABIHF_LINKER="${CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABIHF_LINKER:-arm-linux-gnueabihf-gcc}"
    export CC_armv7_unknown_linux_gnueabihf="${CC_armv7_unknown_linux_gnueabihf:-arm-linux-gnueabihf-gcc}"
    export CXX_armv7_unknown_linux_gnueabihf="${CXX_armv7_unknown_linux_gnueabihf:-arm-linux-gnueabihf-g++}"
    export AR_armv7_unknown_linux_gnueabihf="${AR_armv7_unknown_linux_gnueabihf:-arm-linux-gnueabihf-ar}"
    export PKG_CONFIG_ALLOW_CROSS=1
    export PYO3_CROSS=1
    export PYO3_CROSS_PYTHON_VERSION="${PYO3_CROSS_PYTHON_VERSION:-3.11}"
    maturin build \
      --release \
      --target "$TARGET_TRIPLE" \
      --features sdl2_dynamic \
      --manylinux off \
      -o "$WHEEL_DIR"
  )
}

stage_runtime() {
  local wheel
  wheel="$(find "$WHEEL_DIR" -maxdepth 1 -name 'pyxel-*.whl' | sort | tail -n 1)"
  [ -n "$wheel" ] || die "Pyxel wheel was not produced"

  rm -rf "$DIST_DIR"
  mkdir -p "$SITE_DIR" "$BIN_DIR" "$DOC_DIR" "$WHEEL_COPY_DIR"
  cp "$wheel" "$WHEEL_COPY_DIR/"
  install -m 0644 "$SHIM_SOURCE" "$WHEEL_COPY_DIR/"

  WHEEL_PATH="$wheel" SITE_DIR="$SITE_DIR" python3 - <<'PY'
import os
import zipfile

with zipfile.ZipFile(os.environ["WHEEL_PATH"]) as wheel:
    wheel.extractall(os.environ["SITE_DIR"])
PY

  cat >"${BIN_DIR}/plumos-pyxel-a30" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
PYXEL_SITE="${PLUMOS_PYXEL_A30_SITE:-${PLUMOS_ROOT}/experiments/pyxel-a30-site}"
PYXEL_SHIM_SITE="${PLUMOS_PYXEL_A30_SHIM_SITE:-${PLUMOS_ROOT}/share/pyxel-a30}"
export HOME="${PLUMOS_PYXEL_HOME:-/mnt/SDCARD}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-a30mali}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_OPENGL_LIBRARY="${SDL_OPENGL_LIBRARY:-/usr/lib/libGLESv2.so}"
export SDL_EGL_LIBRARY="${SDL_EGL_LIBRARY:-/usr/lib/libEGL.so}"
export PLUMOS_A30MALI_ROTATION="${PLUMOS_A30MALI_ROTATION:-cw}"
export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7,back:b8,start:b9,guide:b10,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
export PYTHONPATH="${PYXEL_SHIM_SITE}${PYTHONPATH:+:${PYTHONPATH}}"

case "${PLUMOS_PYXEL_USE_PATCHED:-0}" in
  1|yes|YES|true|TRUE|on|ON)
    export PYTHONPATH="${PYXEL_SITE}${PYTHONPATH:+:${PYTHONPATH}}"
    export PLUMOS_PYXEL_A30_PRESENT="${PLUMOS_PYXEL_A30_PRESENT:-1}"
    export PLUMOS_PYXEL_A30_ROTATION="${PLUMOS_PYXEL_A30_ROTATION:-cw}"
    ;;
esac

case "${PLUMOS_PYXEL_INIT_SHIM:-1}" in
  0|no|NO|false|FALSE|off|OFF|none|NONE) ;;
  *) set -- -m plumos_pyxel_a30_shim "$@" ;;
esac

exec "${PLUMOS_ROOT}/lib/ld-linux-armhf.so.3" \
  --library-path "${PLUMOS_ROOT}/python/lib:${PLUMOS_ROOT}/lib:/usr/lib:/lib" \
  --argv0 "${PLUMOS_ROOT}/bin/python3" \
  "${PLUMOS_ROOT}/python/bin/python3.14" "$@"
EOF
  chmod 0755 "${BIN_DIR}/plumos-pyxel-a30"

  cat >"${BIN_DIR}/plumos-pyxel-a30-launch" <<'EOF'
#!/bin/sh
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
LOG_DIR="${PLUMOS_PYXEL_LOG_DIR:-${PLUMOS_ROOT}/logs/pyxel}"
RUN_ID="$(date +%Y%m%d-%H%M%S 2>/dev/null || echo "run-$$")"
JOYSTICKD_OPTS="${PLUMOS_PYXEL_JOYSTICKD_OPTS:---device-mode xbox --trigger-mode buttons --shoulder-layout user}"
CPU_STATE="/tmp/plumos-pyxel-a30-cpustate-$$"
joystickd_pid=
hotkeyd_pid=
app_pid=
cleanup_done=0
cpu_policy_applied=0

mkdir -p "${LOG_DIR}" 2>/dev/null || true

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
  if [ -n "${governor:-}" ] && [ -w "$p/scaling_governor" ]; then
    echo "${governor}" >"$p/scaling_governor" 2>/dev/null || true
  fi
  if [ -n "${min_freq:-}" ] && [ -w "$p/scaling_min_freq" ]; then
    echo "${min_freq}" >"$p/scaling_min_freq" 2>/dev/null || true
  fi
  if [ -n "${max_freq:-}" ] && [ -w "$p/scaling_max_freq" ]; then
    echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
  fi
  rm -f "${CPU_STATE}"
  cpu_policy_applied=0
}

apply_cpu_policy() {
  policy="${1:-keep}"
  freq="${2:-}"
  cores="${3:-keep}"

  case "${policy}" in
    keep|performance|fixed) ;;
    *) echo "error: invalid Pyxel CPU policy: ${policy}" >&2; exit 2 ;;
  esac
  case "${cores}" in
    keep|2|4) ;;
    *) echo "error: invalid Pyxel CPU cores: ${cores}" >&2; exit 2 ;;
  esac
  if [ "${policy}" = fixed ]; then
    case "${freq}" in
      ''|*[!0-9]*) echo "error: invalid Pyxel CPU frequency: ${freq}" >&2; exit 2 ;;
    esac
  fi
  if [ "${policy}" = keep ] && [ "${cores}" = keep ]; then
    return 0
  fi

  save_cpu_policy
  cpu_policy_applied=1

  case "${cores}" in
    2)
      [ -w /sys/devices/system/cpu/cpu1/online ] &&
        echo 1 >/sys/devices/system/cpu/cpu1/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu2/online ] &&
        echo 0 >/sys/devices/system/cpu/cpu2/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu3/online ] &&
        echo 0 >/sys/devices/system/cpu/cpu3/online 2>/dev/null || true
      ;;
    4)
      for cpu in 1 2 3; do
        [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ] &&
          echo 1 >"/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
      done
      ;;
  esac

  p=$(cpu_freq_dir)
  case "${policy}" in
    performance)
      if [ -w "$p/scaling_max_freq" ]; then
        max_freq=$(cat "$p/cpuinfo_max_freq" 2>/dev/null || true)
        [ -n "${max_freq}" ] || max_freq="${freq:-1344000}"
        echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      fi
      echo performance >"$p/scaling_governor" 2>/dev/null || true
      ;;
    fixed)
      echo "${freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      echo "${freq}" >"$p/scaling_min_freq" 2>/dev/null || true
      echo userspace >"$p/scaling_governor" 2>/dev/null || true
      [ -w "$p/scaling_setspeed" ] &&
        echo "${freq}" >"$p/scaling_setspeed" 2>/dev/null || true
      ;;
  esac

  {
    printf 'policy=%s\n' "${policy}"
    printf 'freq=%s\n' "${freq:-keep}"
    printf 'cores=%s\n' "${cores}"
    printf 'online=%s\n' "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
    printf 'governor=%s\n' "$(cat "$p/scaling_governor" 2>/dev/null || true)"
    printf 'cur_freq=%s\n' "$(cat "$p/scaling_cur_freq" 2>/dev/null || true)"
    printf 'min_freq=%s\n' "$(cat "$p/scaling_min_freq" 2>/dev/null || true)"
    printf 'max_freq=%s\n' "$(cat "$p/scaling_max_freq" 2>/dev/null || true)"
  } >"${LOG_DIR}/${RUN_ID}-cpu.log" 2>&1 || true
}

stop_resident_joystickd() {
  for d in /proc/[0-9]*; do
    [ -r "$d/comm" ] || continue
    comm="$(cat "$d/comm" 2>/dev/null || true)"
    case "$comm" in
      plumos-joystick*)
        pid="${d#/proc/}"
        kill -TERM "$pid" >/dev/null 2>&1 || true
        ;;
    esac
  done
  sleep 1
  for d in /proc/[0-9]*; do
    [ -r "$d/comm" ] || continue
    comm="$(cat "$d/comm" 2>/dev/null || true)"
    case "$comm" in
      plumos-joystick*)
        pid="${d#/proc/}"
        kill -KILL "$pid" >/dev/null 2>&1 || true
        ;;
    esac
  done
}

cleanup() {
  [ "$cleanup_done" = 1 ] && return
  cleanup_done=1

  if [ -n "${app_pid:-}" ] && kill -0 "$app_pid" >/dev/null 2>&1; then
    kill -TERM "$app_pid" >/dev/null 2>&1 || true
  fi
  if [ -n "${hotkeyd_pid:-}" ] && kill -0 "$hotkeyd_pid" >/dev/null 2>&1; then
    kill -TERM "$hotkeyd_pid" >/dev/null 2>&1 || true
  fi
  if [ -n "${joystickd_pid:-}" ] && kill -0 "$joystickd_pid" >/dev/null 2>&1; then
    kill -TERM "$joystickd_pid" >/dev/null 2>&1 || true
  fi
  sleep 1
  if [ -n "${app_pid:-}" ] && kill -0 "$app_pid" >/dev/null 2>&1; then
    kill -KILL "$app_pid" >/dev/null 2>&1 || true
  fi
  if [ -n "${hotkeyd_pid:-}" ] && kill -0 "$hotkeyd_pid" >/dev/null 2>&1; then
    kill -KILL "$hotkeyd_pid" >/dev/null 2>&1 || true
  fi
  if [ -n "${joystickd_pid:-}" ] && kill -0 "$joystickd_pid" >/dev/null 2>&1; then
    kill -KILL "$joystickd_pid" >/dev/null 2>&1 || true
  fi
  restore_cpu_policy

  if [ -x "${PLUMOS_ROOT}/bin/plumos-volume-control" ]; then
    "${PLUMOS_ROOT}/bin/plumos-volume-control" persist-runtime \
      >>"${LOG_DIR}/${RUN_ID}-volume.log" 2>&1 || true
  fi
}

trap 'cleanup' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

if [ -x "${PLUMOS_ROOT}/bin/plumos-volume-control" ]; then
  "${PLUMOS_ROOT}/bin/plumos-volume-control" apply \
    >>"${LOG_DIR}/${RUN_ID}-volume.log" 2>&1 || true
fi

apply_cpu_policy "${PLUMOS_PYXEL_CPU_POLICY:-keep}" \
  "${PLUMOS_PYXEL_CPU_FREQ:-}" \
  "${PLUMOS_PYXEL_CPU_CORES:-keep}"

case "${PLUMOS_PYXEL_JOYSTICKD:-auto}" in
  0|no|NO|false|FALSE|none) ;;
  *)
    if [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ]; then
      case "${PLUMOS_PYXEL_JOYSTICKD_REPLACE:-1}" in
        0|no|NO|false|FALSE) ;;
        *) stop_resident_joystickd ;;
      esac
      "${PLUMOS_ROOT}/bin/plumos-joystickd" ${JOYSTICKD_OPTS} \
        >"${LOG_DIR}/${RUN_ID}-joystickd.log" 2>&1 &
      joystickd_pid=$!
      sleep 1
    fi
    ;;
esac

case "${PLUMOS_PYXEL_VOLUME_HOTKEYD:-auto}" in
  0|no|NO|false|FALSE|none) ;;
  *)
    if [ -x "${PLUMOS_ROOT}/bin/plumos-safe-hotkeyd" ]; then
      "${PLUMOS_ROOT}/bin/plumos-safe-hotkeyd" --volume-only \
        --log "${LOG_DIR}/${RUN_ID}-safe-hotkeyd.log" \
        >"${LOG_DIR}/${RUN_ID}-safe-hotkeyd.stdout" 2>&1 &
      hotkeyd_pid=$!
      sleep 1
    fi
    ;;
esac

"${PLUMOS_ROOT}/bin/plumos-pyxel-a30" "$@" &
app_pid=$!
wait "$app_pid"
app_rc=$?
app_pid=
cleanup
trap - EXIT INT TERM HUP
exit "$app_rc"
EOF
  chmod 0755 "${BIN_DIR}/plumos-pyxel-a30-launch"

  {
    echo "plumOS Pyxel A30 runtime"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "pyxel_version: ${PYXEL_VERSION}"
    echo "pyxel_ref: ${PYXEL_REF}"
    echo "target: ${TARGET_TRIPLE}"
    echo "rust_toolchain: ${RUST_TOOLCHAIN}"
    echo "patch: $(basename "$PYXEL_PATCH")"
    echo
    sha256sum "$wheel"
    sha256sum "${BIN_DIR}/plumos-pyxel-a30" "${BIN_DIR}/plumos-pyxel-a30-launch"
    sha256sum "$SHIM_SOURCE"
    echo
    find "$SITE_DIR/pyxel" -maxdepth 1 -type f | sort | xargs -r sha256sum
  } >"${DOC_DIR}/manifest.txt"
}

ensure_rustup
prepare_source
build_wheel
stage_runtime

printf 'Built %s\n' "$DIST_DIR"
