#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DURATION=8
DEPLOY=0
RESTART_FE=1
ROTATION=ccw

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Loads the plumOS fceumm and gambatte libretro cores with one NES and one GB ROM
on the A30. The probe stops the plumOS FE while RetroArch owns the framebuffer.

Options:
  --deploy             Deploy dist/plumos-libretro-cores before probing.
  --duration SECONDS   Runtime per core before killing RetroArch. Default: ${DURATION}
  --rotation VALUE     PLUMOS_RA_DISPLAY_ROTATION value. Default: ${ROTATION}
  --no-restart-fe      Leave the plumOS FE stopped after the probe.
  -h, --help           Show this help.
EOF
}

shell_quote() {
  local value="$1"
  printf "'%s'" "${value//\'/\'\\\'\'}"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      ;;
    --duration)
      DURATION="${2:?missing duration}"
      shift
      ;;
    --rotation)
      ROTATION="${2:?missing rotation}"
      shift
      ;;
    --no-restart-fe)
      RESTART_FE=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

case "$DURATION" in
  ''|*[!0-9]*)
    echo "error: --duration must be an integer number of seconds" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-libretro-cores" /mnt/SDCARD
fi

remote_script='
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
RA="$PLUMOS_ROOT/retroarch/bin/retroarch"
CFG="$PLUMOS_ROOT/retroarch/config/retroarch-minimal.cfg"
APPEND=/tmp/plumos-libretro-core-smoke.cfg
FE="$PLUMOS_ROOT/bin/plumos-controller-ui-mali"
LOG_DIR="$PLUMOS_ROOT/retroarch/logs"
RA_PID=""
FAILED=0

proc_cmdline() {
  pid="$1"
  tr "\000" " " <"/proc/$pid/cmdline" 2>/dev/null || true
}

proc_pids_matching() {
  pattern="$1"
  for proc in /proc/[0-9]*; do
    [ -d "$proc" ] || continue
    pid="${proc##*/}"
    [ "$pid" = "$$" ] && continue
    cmd="$(proc_cmdline "$pid")"
    [ -n "$cmd" ] || continue
    case "$cmd" in
      *"$pattern"*) printf "%s\n" "$pid" ;;
    esac
  done
}

fe_pids() {
  {
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" || true
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-controller-ui-mali.bin" || true
  } | awk "!seen[\$0]++"
}

retroarch_pids() {
  {
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-retroarch-minimal" || true
    proc_pids_matching "$PLUMOS_ROOT/retroarch/bin/retroarch" || true
    proc_pids_matching "$PLUMOS_ROOT/retroarch/bin/retroarch.bin" || true
  } | awk "!seen[\$0]++"
}

kill_pids() {
  pids="$1"
  [ -n "$pids" ] || return 0
  kill $pids 2>/dev/null || true
  sleep 1
  kill -KILL $pids 2>/dev/null || true
}

cleanup() {
  if [ -n "$RA_PID" ] && kill -0 "$RA_PID" 2>/dev/null; then
    kill "$RA_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$RA_PID" 2>/dev/null || true
  fi

  if [ "$PLUMOS_LIBRETRO_CORE_SMOKE_RESTART_FE" = "1" ] &&
     [ -x "$FE" ] &&
     [ -z "$(fe_pids)" ]; then
    echo "== restart plumOS FE =="
    PLUMOS_FRONTEND_MODE=manual "$FE" --rotation auto >/tmp/plumos-frontend-resume.log 2>&1 </dev/null &
  fi
}

first_rom() {
  dir="$1"
  pattern="$2"
  find "$dir" -maxdepth 2 -type f -name "$pattern" 2>/dev/null |
    grep -v "/\\._" |
    head -n 1
}

run_case() {
  name="$1"
  core="$2"
  rom="$3"
  log="/tmp/plumos-libretro-${name}.log"
  last_log="$LOG_DIR/libretro-${name}-last.log"

  echo "== run ${name} =="
  echo "core=$core"
  echo "rom=$rom"
  rm -f "$log"
  set +e
  PLUMOS_RA_DISPLAY_ROTATION="$PLUMOS_LIBRETRO_CORE_SMOKE_ROTATION" \
    "$RA" --verbose --config "$CFG" --appendconfig "$APPEND" -L "$core" "$rom" >"$log" 2>&1 &
  RA_PID=$!
  sleep "$PLUMOS_LIBRETRO_CORE_SMOKE_DURATION"
  if kill -0 "$RA_PID" 2>/dev/null; then
    kill "$RA_PID" 2>/dev/null || true
    sleep 1
    wait "$RA_PID" 2>/dev/null
    rc=$?
    survived=1
  else
    wait "$RA_PID" 2>/dev/null
    rc=$?
    survived=0
  fi
  set -e
  RA_PID=""

  cp "$log" "$last_log" 2>/dev/null || true
  grep -Ei "RetroArch|libretro|core|content|video|rotation|context|EGL|GLES|Mali|error|failed|segmentation|fatal" "$log" |
    tail -n 120 || true
  tail -n 40 "$log" || true

  if [ "$survived" = "1" ]; then
    echo "check=${name} status=pass survived=${PLUMOS_LIBRETRO_CORE_SMOKE_DURATION}s wait_rc=${rc}"
  else
    echo "check=${name} status=fail exited_rc=${rc}"
    FAILED=1
  fi
}

trap cleanup EXIT

mkdir -p "$LOG_DIR"
cat > "$APPEND" <<EOF
video_rotation = "1"
screen_orientation = "1"
config_save_on_exit = "false"
EOF

NES_ROM="$(first_rom /mnt/SDCARD/Roms/FC "*.nes")"
GB_ROM="$(first_rom /mnt/SDCARD/Roms/GB "*.gb")"

if [ -z "$NES_ROM" ]; then
  echo "error: no NES ROM found under /mnt/SDCARD/Roms/FC" >&2
  exit 1
fi
if [ -z "$GB_ROM" ]; then
  echo "error: no GB ROM found under /mnt/SDCARD/Roms/GB" >&2
  exit 1
fi

echo "== binaries =="
ls -l \
  "$RA" \
  "$CFG" \
  "$PLUMOS_ROOT/retroarch/cores/fceumm_libretro.so" \
  "$PLUMOS_ROOT/retroarch/cores/gambatte_libretro.so" 2>/dev/null || true
echo "== display processes before =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch" | grep -v grep || true

old_ra_pids="$(retroarch_pids)"
[ -n "$old_ra_pids" ] && kill_pids "$old_ra_pids"

fe_pids_before="$(fe_pids)"
if [ -n "$fe_pids_before" ]; then
  echo "== stop plumOS FE for framebuffer ownership =="
  kill_pids "$fe_pids_before"
fi

run_case fceumm "$PLUMOS_ROOT/retroarch/cores/fceumm_libretro.so" "$NES_ROM"
run_case gambatte "$PLUMOS_ROOT/retroarch/cores/gambatte_libretro.so" "$GB_ROM"

echo "== display processes after =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch" | grep -v grep || true

if [ "$FAILED" = "0" ]; then
  echo "result=libretro_core_smoke_ok"
  exit 0
fi

echo "result=libretro_core_smoke_issue"
exit 1
'

A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/run-a30.sh" \
  "PLUMOS_LIBRETRO_CORE_SMOKE_DURATION=$(shell_quote "$DURATION")
PLUMOS_LIBRETRO_CORE_SMOKE_RESTART_FE=$(shell_quote "$RESTART_FE")
PLUMOS_LIBRETRO_CORE_SMOKE_ROTATION=$(shell_quote "$ROTATION")
$remote_script"
