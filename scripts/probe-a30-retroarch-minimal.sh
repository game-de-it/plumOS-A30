#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_RETROARCH_MINIMAL:-0}"
DURATION="${PLUMOS_RETROARCH_MINIMAL_DURATION:-5}"
STOP_MAINUI="${PLUMOS_RETROARCH_MINIMAL_STOP_MAINUI:-1}"
STOP_KEYMON="${PLUMOS_RETROARCH_MINIMAL_STOP_KEYMON:-1}"
RESTART_STOCK="${PLUMOS_RETROARCH_MINIMAL_RESTART_STOCK:-0}"
RESTART_FE="${PLUMOS_RETROARCH_MINIMAL_RESTART_FE:-1}"
ROTATION="${PLUMOS_RETROARCH_MINIMAL_ROTATION:-none}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--deploy] [--duration SEC] [--rotation none|auto|90|180|270|ccw|cw] [--stop-mainui] [--stop-keymon] [--restart-stock] [--no-restart-fe]

Runs the minimal RetroArch GLES + Mali fbdev RGUI display probe on the A30.
Defaults are plumOS-oriented: stock MainUI/keymon are stopped and not restarted.

Environment:
  PLUMOS_DEPLOY_RETROARCH_MINIMAL       Deploy dist/plumos-retroarch-minimal first. Default: ${DEPLOY}
  PLUMOS_RETROARCH_MINIMAL_DURATION     Display duration in seconds. Default: ${DURATION}
  PLUMOS_RETROARCH_MINIMAL_STOP_MAINUI  Stop stock MainUI/supervisor. Default: ${STOP_MAINUI}
  PLUMOS_RETROARCH_MINIMAL_STOP_KEYMON  Stop stock keymon. Default: ${STOP_KEYMON}
  PLUMOS_RETROARCH_MINIMAL_RESTART_STOCK Resume/restart stock processes after probe. Default: ${RESTART_STOCK}
  PLUMOS_RETROARCH_MINIMAL_RESTART_FE   Start/resume plumOS FE after probe. Default: ${RESTART_FE}
  PLUMOS_RETROARCH_MINIMAL_ROTATION     RetroArch video_rotation override. Default: ${ROTATION}
EOF
}

shell_quote() {
  local s="$1"
  printf "'%s'" "${s//\'/\'\\\'\'}"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --duration)
      DURATION="${2:?missing --duration value}"
      shift 2
      ;;
    --rotation)
      ROTATION="${2:?missing --rotation value}"
      shift 2
      ;;
    --stop-mainui)
      STOP_MAINUI=1
      shift
      ;;
    --no-stop-mainui)
      STOP_MAINUI=0
      shift
      ;;
    --stop-keymon)
      STOP_KEYMON=1
      shift
      ;;
    --no-stop-keymon)
      STOP_KEYMON=0
      shift
      ;;
    --restart-stock)
      RESTART_STOCK=1
      shift
      ;;
    --no-restart-stock)
      RESTART_STOCK=0
      shift
      ;;
    --restart-fe)
      RESTART_FE=1
      shift
      ;;
    --no-restart-fe)
      RESTART_FE=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

for value_name in DEPLOY STOP_MAINUI STOP_KEYMON RESTART_STOCK RESTART_FE; do
  value="${!value_name}"
  case "$value" in
    0|1) ;;
    *)
      echo "error: ${value_name} must be 0 or 1: ${value}" >&2
      exit 2
      ;;
  esac
done

case "$DURATION" in
  ''|*[!0-9]*)
    echo "error: --duration must be a non-negative integer: $DURATION" >&2
    exit 2
    ;;
esac

case "$ROTATION" in
  none|normal|0)
    ROTATION_VALUE=0
    ROTATION_LABEL=none
    ;;
  auto|ccw|90|left)
    ROTATION_VALUE=1
    ROTATION_LABEL=ccw
    ;;
  180)
    ROTATION_VALUE=2
    ROTATION_LABEL=180
    ;;
  cw|270|right)
    ROTATION_VALUE=3
    ROTATION_LABEL=cw
    ;;
  *)
    echo "error: --rotation must be none, auto, 90, 180, 270, ccw, or cw: $ROTATION" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-retroarch-minimal" /mnt/SDCARD
fi

remote_script='
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
export PLUMOS_ROOT
RA="$PLUMOS_ROOT/bin/plumos-retroarch-minimal"
RA_BIN="$PLUMOS_ROOT/retroarch/bin/retroarch.bin"
RA_LOG=/tmp/plumos-retroarch-minimal.log
RA_LAST_LOG="$PLUMOS_ROOT/retroarch/logs/minimal-last.log"
RA_APPEND=/tmp/plumos-retroarch-minimal-append.cfg
FE="$PLUMOS_ROOT/bin/plumos-controller-ui-mali"
MAINUI_DIR=/mnt/SDCARD/miyoo/app
STOCK_MAINUI="$MAINUI_DIR/MainUI.stock"
STOCK_KEYMON="$MAINUI_DIR/keymon"
MAINUI_WAS_RUNNING=0
KEYMON_WAS_RUNNING=0
FE_WAS_RUNNING=0
MAIN_SUPERVISOR_PIDS=""
MAIN_SUPERVISOR_STOPPED=0
RA_PID=""

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

mainui_pids() {
  proc_pids_matching "/mnt/SDCARD/miyoo/app/MainUI" || true
}

main_supervisor_pids() {
  proc_pids_matching "/bin/sh /etc/main" || true
}

keymon_pids() {
  proc_pids_matching "/mnt/SDCARD/miyoo" |
    while read -r pid; do
      [ -n "$pid" ] || continue
      case "$(proc_cmdline "$pid")" in
        *"/app/keymon"*) printf "%s\n" "$pid" ;;
      esac
    done || true
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

  if [ "$PLUMOS_RETROARCH_MINIMAL_RESTART_FE" = "1" ] &&
     [ -x "$FE" ] &&
     [ -z "$(fe_pids)" ]; then
    echo "== restart plumOS FE =="
    PLUMOS_FRONTEND_MODE=manual "$FE" --rotation auto >/tmp/plumos-frontend-resume.log 2>&1 </dev/null &
  fi

  if [ "$MAIN_SUPERVISOR_STOPPED" = "1" ] && [ -n "$MAIN_SUPERVISOR_PIDS" ]; then
    if [ "$PLUMOS_RETROARCH_MINIMAL_RESTART_STOCK" = "1" ]; then
      echo "== resume stock main supervisor =="
      kill -CONT $MAIN_SUPERVISOR_PIDS 2>/dev/null || true
      sleep 1
    else
      echo "== leave stock main supervisor stopped =="
      kill -KILL $MAIN_SUPERVISOR_PIDS 2>/dev/null || true
      sleep 1
    fi
  fi

  if [ "$PLUMOS_RETROARCH_MINIMAL_RESTART_STOCK" = "1" ]; then
    if [ "$MAINUI_WAS_RUNNING" = "1" ] && [ -x "$STOCK_MAINUI" ] && [ -z "$(mainui_pids)" ]; then
      echo "== restart stock MainUI =="
      (cd "$MAINUI_DIR" && "$STOCK_MAINUI" >/tmp/plumos-mainui-restart.log 2>&1 &)
    fi
    if [ "$KEYMON_WAS_RUNNING" = "1" ] && [ -x "$STOCK_KEYMON" ] && [ -z "$(keymon_pids)" ]; then
      echo "== restart stock keymon =="
      (cd "$MAINUI_DIR" && "$STOCK_KEYMON" >/tmp/plumos-keymon-restart.log 2>&1 &)
    fi
  fi
}

trap cleanup EXIT

mkdir -p "$PLUMOS_ROOT/retroarch/logs"
rm -f "$RA_LOG"
cat > "$RA_APPEND" <<EOF
video_rotation = "$PLUMOS_RETROARCH_MINIMAL_ROTATION_VALUE"
screen_orientation = "$PLUMOS_RETROARCH_MINIMAL_ROTATION_VALUE"
config_save_on_exit = "false"
EOF

echo "== device inventory =="
ls -l /dev/fb* /dev/mali /dev/disp /dev/ion 2>/dev/null || true
echo "== gpu libraries =="
ls -l /usr/lib/libEGL.so /usr/lib/libGLESv2.so /usr/lib/libGLESv2.so.2 /usr/lib/libMali.so 2>/dev/null || true
echo "== retroarch binaries =="
ls -l "$RA" "$RA_BIN" "$PLUMOS_ROOT/retroarch/config/retroarch-minimal.cfg" 2>/dev/null || true
echo "== display/input processes before =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch" | grep -v grep || true

MAINUI_PIDS="$(mainui_pids)"
KEYMON_PIDS="$(keymon_pids)"
FE_PIDS="$(fe_pids)"
RA_PIDS="$(retroarch_pids)"

[ -n "$MAINUI_PIDS" ] && MAINUI_WAS_RUNNING=1
[ -n "$KEYMON_PIDS" ] && KEYMON_WAS_RUNNING=1
[ -n "$FE_PIDS" ] && FE_WAS_RUNNING=1

if [ "$PLUMOS_RETROARCH_MINIMAL_STOP_MAINUI" = "1" ] ||
   [ "$PLUMOS_RETROARCH_MINIMAL_STOP_KEYMON" = "1" ]; then
  echo "== stop stock MainUI/keymon =="
  MAIN_SUPERVISOR_PIDS="$(main_supervisor_pids)"
  if [ -n "$MAIN_SUPERVISOR_PIDS" ]; then
    kill -STOP $MAIN_SUPERVISOR_PIDS 2>/dev/null || true
    MAIN_SUPERVISOR_STOPPED=1
    sleep 1
  fi
  if [ "$PLUMOS_RETROARCH_MINIMAL_STOP_MAINUI" = "1" ]; then
    kill_pids "$MAINUI_PIDS"
  fi
  if [ "$PLUMOS_RETROARCH_MINIMAL_STOP_KEYMON" = "1" ]; then
    kill_pids "$KEYMON_PIDS"
  fi
fi

if [ -n "$RA_PIDS" ]; then
  echo "== stop old RetroArch probe =="
  kill_pids "$RA_PIDS"
fi

if [ -n "$FE_PIDS" ]; then
  echo "== stop plumOS FE for framebuffer ownership =="
  kill_pids "$FE_PIDS"
fi

echo "== processes before RetroArch =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch" | grep -v grep || true

echo "== run RetroArch minimal display probe =="
echo "rotation=${PLUMOS_RETROARCH_MINIMAL_ROTATION_LABEL} video_rotation=${PLUMOS_RETROARCH_MINIMAL_ROTATION_VALUE}"
set +e
PLUMOS_RA_DISPLAY_ROTATION="$PLUMOS_RETROARCH_MINIMAL_ROTATION_LABEL" \
  "$RA" --appendconfig "$RA_APPEND" >"$RA_LOG" 2>&1 &
RA_PID=$!
sleep "$PLUMOS_RETROARCH_MINIMAL_DURATION"
if kill -0 "$RA_PID" 2>/dev/null; then
  kill "$RA_PID" 2>/dev/null || true
  sleep 1
  wait "$RA_PID" 2>/dev/null
  wait_rc=$?
  survived=1
else
  wait "$RA_PID" 2>/dev/null
  wait_rc=$?
  survived=0
fi
set -e
RA_PID=""

cp "$RA_LOG" "$RA_LAST_LOG" 2>/dev/null || true
echo "== retroarch log highlights =="
grep -Ei "RetroArch|video|rotation|context|EGL|GLES|Mali|RGUI|driver|error|failed|segmentation|fatal" "$RA_LOG" | tail -n 120 || true
echo "== retroarch log tail =="
tail -n 80 "$RA_LOG" || true
echo "== display/input processes after =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch" | grep -v grep || true

if [ "$survived" = "1" ]; then
  echo "result=retroarch_minimal_survived_${PLUMOS_RETROARCH_MINIMAL_DURATION}s"
  exit 0
fi

echo "result=retroarch_minimal_exited_rc_${wait_rc}"
exit "$wait_rc"
'

A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/run-a30.sh" \
  "PLUMOS_RETROARCH_MINIMAL_DURATION=$(shell_quote "$DURATION")
PLUMOS_RETROARCH_MINIMAL_STOP_MAINUI=$(shell_quote "$STOP_MAINUI")
PLUMOS_RETROARCH_MINIMAL_STOP_KEYMON=$(shell_quote "$STOP_KEYMON")
PLUMOS_RETROARCH_MINIMAL_RESTART_STOCK=$(shell_quote "$RESTART_STOCK")
PLUMOS_RETROARCH_MINIMAL_RESTART_FE=$(shell_quote "$RESTART_FE")
PLUMOS_RETROARCH_MINIMAL_ROTATION_VALUE=$(shell_quote "$ROTATION_VALUE")
PLUMOS_RETROARCH_MINIMAL_ROTATION_LABEL=$(shell_quote "$ROTATION_LABEL")
$remote_script"
