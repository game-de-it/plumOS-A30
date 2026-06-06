#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD_RESIDENCY:-0}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-30000}"
FE_TIMEOUT="${PLUMOS_RESIDENCY_FE_TIMEOUT:-3}"
SDL2_RUN_MS="${PLUMOS_RESIDENCY_SDL2_MS:-2500}"
PPSSPP_RUN_MS="${PLUMOS_RESIDENCY_PPSSPP_MS:-4000}"
PPSSPP_ROM="${PLUMOS_PSP_ROM:-/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso}"
RUN_PPSSPP="${PLUMOS_RESIDENCY_PPSSPP:-1}"
STOP_STOCK="${PLUMOS_RESIDENCY_STOP_STOCK:-1}"
ROTATION="${PLUMOS_FRONTEND_MALI_ROTATION:-auto}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--timeout-ms MS] [--fe-timeout SEC]
       [--sdl2-ms MS] [--ppsspp-ms MS] [--rom PATH] [--skip-ppsspp]
       [--keep-stock] [--rotation auto|none|cw|ccw]

Runs a stockless plumOS residency probe for plumos-joystickd --device-mode xbox.
The probe starts joystickd once, exercises the Mali frontend, checks SDL2
GameController visibility, optionally launches stock PPSSPPSDL directly, and
then verifies that the virtual gamepad/process/fds are cleaned up.

Environment:
  A30_TARGET                         SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_JOYSTICKD_RESIDENCY Deploy frontend, SDL2 probe, and joystickd.
  PLUMOS_JOYSTICKD_TIMEOUT_MS       joystickd runtime. Default: ${JOYSTICKD_TIMEOUT_MS}
  PLUMOS_RESIDENCY_FE_TIMEOUT       FE display duration. Default: ${FE_TIMEOUT}
  PLUMOS_RESIDENCY_SDL2_MS          SDL2 probe duration. Default: ${SDL2_RUN_MS}
  PLUMOS_RESIDENCY_PPSSPP_MS        PPSSPP direct launch duration. Default: ${PPSSPP_RUN_MS}
  PLUMOS_PSP_ROM                    PSP ROM path. Default: ${PPSSPP_ROM}
  PLUMOS_RESIDENCY_PPSSPP=0         Skip PPSSPP direct launch.
  PLUMOS_RESIDENCY_STOP_STOCK=0     Do not stop stock MainUI/keymon first.
  PLUMOS_FRONTEND_MALI_ROTATION     auto, none, cw, or ccw. Default: ${ROTATION}
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
    --timeout-ms)
      JOYSTICKD_TIMEOUT_MS="${2:?missing --timeout-ms value}"
      shift 2
      ;;
    --fe-timeout)
      FE_TIMEOUT="${2:?missing --fe-timeout value}"
      shift 2
      ;;
    --sdl2-ms)
      SDL2_RUN_MS="${2:?missing --sdl2-ms value}"
      shift 2
      ;;
    --ppsspp-ms)
      PPSSPP_RUN_MS="${2:?missing --ppsspp-ms value}"
      shift 2
      ;;
    --rom)
      PPSSPP_ROM="${2:?missing --rom value}"
      shift 2
      ;;
    --skip-ppsspp)
      RUN_PPSSPP=0
      shift
      ;;
    --keep-stock)
      STOP_STOCK=0
      shift
      ;;
    --rotation)
      ROTATION="${2:?missing --rotation value}"
      shift 2
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

case "$DEPLOY" in 0|1) ;; *) echo "error: deploy must be 0 or 1" >&2; exit 2 ;; esac
case "$RUN_PPSSPP" in 0|1) ;; *) echo "error: PPSSPP flag must be 0 or 1" >&2; exit 2 ;; esac
case "$STOP_STOCK" in 0|1) ;; *) echo "error: stop-stock flag must be 0 or 1" >&2; exit 2 ;; esac
case "$ROTATION" in auto|none|cw|ccw) ;; *) echo "error: invalid rotation: $ROTATION" >&2; exit 2 ;; esac

for value_name in JOYSTICKD_TIMEOUT_MS FE_TIMEOUT SDL2_RUN_MS PPSSPP_RUN_MS; do
  value="${!value_name}"
  case "$value" in
    ""|*[!0-9]*)
      echo "error: $value_name must be an integer: $value" >&2
      exit 2
      ;;
  esac
done

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-frontend" /mnt/SDCARD
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-sdl2-probe" /mnt/SDCARD
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

remote_script='
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
UI="$PLUMOS_ROOT/bin/plumos-controller-ui-mali"
SDL2_PROBE="$PLUMOS_ROOT/bin/plumos-sdl2-probe"
JOYSTICKD="$PLUMOS_ROOT/bin/plumos-joystickd"
PPSSPP_DIR=/mnt/SDCARD/Emu/PPSSPP
PPSSPP_BIN="$PPSSPP_DIR/PPSSPPSDL"
JOY_LOG=/tmp/plumos-residency-joystickd.log
FE_LOG=/tmp/plumos-residency-fe.log
SDL2_LOG=/tmp/plumos-residency-sdl2.log
PPSSPP_LOG=/tmp/plumos-residency-ppsspp.log
ok=1
jpid=""
fpid=""
ppid=""

cleanup() {
  for pid in "$fpid" "$ppid" "$jpid"; do
    [ -n "$pid" ] || continue
    kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT

kill_pids() {
  pids="$1"
  [ -n "$pids" ] || return 0
  kill $pids 2>/dev/null || true
  sleep 1
  kill -KILL $pids 2>/dev/null || true
}

main_supervisor_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"/etc/main"*) echo "$pid" ;;
    esac
  done
}

mainui_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"/mnt/SDCARD/miyoo/app/MainUI"*) echo "$pid" ;;
    esac
  done
}

keymon_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"/mnt/SDCARD/miyoo/"*"/app/keymon"*|*"/mnt/SDCARD/miyoo//app/keymon"*) echo "$pid" ;;
    esac
  done
}

frontend_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"plumos-controller-ui-mali"*|*"plumos-controller-ui-mali.bin"*) echo "$pid" ;;
    esac
  done
}

runtime_probe_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"PPSSPPSDL"*|*"miyoo282_xpad_inputd"*|*"plumos-joystickd"*) echo "$pid" ;;
    esac
  done
}

ppsspp_pids() {
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [ "$pid" != "$$" ] || continue
    cmd=$(cat "$p/cmdline" 2>/dev/null | tr "\000" " " || true)
    case "$cmd" in ash\ -c*|sh\ -c*|/bin/sh\ -c*) continue ;; esac
    case "$cmd" in
      *"PPSSPPSDL"*|*"miyoo282_xpad_inputd"*) echo "$pid" ;;
    esac
  done
}

detect_gamepad() {
  event_path=""
  js_path=""
  for e in /sys/class/input/event*; do
    [ -r "$e/device/name" ] || continue
    [ "$(cat "$e/device/name")" = "plumOS A30 Gamepad" ] || continue
    event_path="/dev/input/${e##*/}"
  done
  for j in /sys/class/input/js*; do
    [ -r "$j/device/name" ] || continue
    [ "$(cat "$j/device/name")" = "plumOS A30 Gamepad" ] || continue
    js_path="/dev/input/${j##*/}"
  done
}

print_fd_holders() {
  label="$1"
  echo "== fd holders: $label =="
  for fd in /proc/[0-9]*/fd/*; do
    target=$(readlink "$fd" 2>/dev/null) || continue
    case "$target" in
      /dev/input/*|/dev/uinput|/dev/ttyS0|*"/dev/input/"*|*"/dev/uinput"*|*"/dev/ttyS0"*)
        pid=${fd#/proc/}
        pid=${pid%%/*}
        cmd=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr "\000" " " || true)
        [ -n "$cmd" ] || cmd=$(cat "/proc/$pid/comm" 2>/dev/null || true)
        case "$cmd" in
          *plumos*|*PPSSPP*|*MainUI*|*keymon*|*xpad*|*retroarch*)
            echo "$pid $cmd $fd -> $target"
            ;;
        esac
        ;;
    esac
  done | sort
}

print_processes() {
  label="$1"
  echo "== processes: $label =="
  ps w 2>/dev/null |
    grep -E "MainUI|keymon|plumos-controller-ui|plumos-joystickd|PPSSPP|xpad|wpa_supplicant|dropbear|/etc/main" |
    grep -v grep || true
}

rm -f "$JOY_LOG" "$FE_LOG" "$SDL2_LOG" "$PPSSPP_LOG"

print_processes before
print_fd_holders before

if [ "$PLUMOS_RESIDENCY_STOP_STOCK" = "1" ]; then
  echo "== stop stock/plumOS display processes for residency probe =="
  main_pids="$(main_supervisor_pids)"
  if [ -n "$main_pids" ]; then
    kill -STOP $main_pids 2>/dev/null || true
  fi
  kill_pids "$(mainui_pids)"
  kill_pids "$(keymon_pids)"
  kill_pids "$(frontend_pids)"
  kill_pids "$(runtime_probe_pids)"
fi

print_processes stockless

"$JOYSTICKD" --device-mode xbox --timeout-ms "$PLUMOS_JOYSTICKD_TIMEOUT_MS" >"$JOY_LOG" 2>&1 &
jpid=$!

i=0
detect_gamepad
while [ "$i" -lt 30 ] && { [ -z "$event_path" ] || [ -z "$js_path" ]; }; do
  sleep 0.2
  detect_gamepad
  i=$((i + 1))
done

echo "detected js=$js_path event=$event_path joystickd_pid=$jpid"
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p" || true
if [ -z "$event_path" ] || [ -z "$js_path" ]; then
  echo "check=gamepad_detected status=fail"
  ok=0
else
  echo "check=gamepad_detected status=pass"
fi
print_fd_holders joystickd_running

set +e
"$UI" --timeout "$PLUMOS_RESIDENCY_FE_TIMEOUT" \
  --rotation "$PLUMOS_FRONTEND_MALI_ROTATION" >"$FE_LOG" 2>&1 &
fpid=$!
sleep 1
print_fd_holders fe_running
wait "$fpid"
fe_rc=$?
fpid=""
set -e

echo "== FE log =="
sed -n "1,160p" "$FE_LOG" || true
echo "frontend_rc=$fe_rc"
if [ "$fe_rc" -eq 0 ]; then
  echo "check=frontend_with_joystickd status=pass"
else
  echo "check=frontend_with_joystickd status=fail"
  ok=0
fi

set +e
SDL_VIDEODRIVER=dummy \
SDL_AUDIODRIVER=dummy \
PLUMOS_ROOT="$PLUMOS_ROOT" \
  "$SDL2_PROBE" --timeout-ms "$PLUMOS_RESIDENCY_SDL2_MS" >"$SDL2_LOG" 2>&1
sdl_rc=$?
set -e

echo "== SDL2 probe log =="
sed -n "1,220p" "$SDL2_LOG" || true
echo "sdl2_rc=$sdl_rc"
if [ "$sdl_rc" -eq 0 ] && grep -q "is_controller=yes" "$SDL2_LOG"; then
  echo "check=sdl2_gamecontroller_with_resident_joystickd status=pass"
else
  echo "check=sdl2_gamecontroller_with_resident_joystickd status=fail"
  ok=0
fi
print_fd_holders after_sdl2_probe

ppsspp_status=skipped
if [ "$PLUMOS_RESIDENCY_PPSSPP" = "1" ]; then
  if [ -f "$PLUMOS_PSP_ROM" ] && [ -x "$PPSSPP_BIN" ]; then
    kill_pids "$(ppsspp_pids)"
    cd "$PPSSPP_DIR"
    set +e
    HOME=/mnt/SDCARD \
    LD_LIBRARY_PATH=/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/Emu/PPSSPP:/lib:/usr/lib \
      "$PPSSPP_BIN" "$PLUMOS_PSP_ROM" >"$PPSSPP_LOG" 2>&1 &
    ppid=$!
    set -e
    sleep_sec=$((PLUMOS_RESIDENCY_PPSSPP_MS / 1000))
    [ "$sleep_sec" -gt 0 ] || sleep_sec=1
    sleep "$sleep_sec"
    print_fd_holders ppsspp_running
    kill "$ppid" 2>/dev/null || true
    wait "$ppid" 2>/dev/null || true
    ppid=""
    echo "== PPSSPP input log =="
    grep -E "loading control pad mappings|found control pad|mapping|pad 1|setUpController|serial open|SDL version" "$PPSSPP_LOG" || true
    if grep -Eq "pad 1 has been assigned|found control pad" "$PPSSPP_LOG"; then
      ppsspp_status=pass
    else
      ppsspp_status=fail
      ok=0
    fi
  else
    echo "ppsspp_skip rom=$PLUMOS_PSP_ROM bin=$PPSSPP_BIN"
  fi
fi
echo "check=ppsspp_direct_with_resident_joystickd status=$ppsspp_status"

echo "== wait joystickd timeout =="
wait "$jpid" || true
jpid=""

echo "== joystickd log =="
sed -n "1,220p" "$JOY_LOG" || true

detect_gamepad
if [ -n "$event_path" ] || [ -n "$js_path" ]; then
  echo "check=gamepad_removed_after_joystickd_exit status=fail js=$js_path event=$event_path"
  ok=0
else
  echo "check=gamepad_removed_after_joystickd_exit status=pass"
fi

print_fd_holders after_joystickd_exit
print_processes after

if runtime_probe_pids | grep -q .; then
  echo "check=stale_processes status=fail"
  ok=0
else
  echo "check=stale_processes status=pass"
fi

rm -f "$JOY_LOG" "$FE_LOG" "$SDL2_LOG" "$PPSSPP_LOG"

if [ "$ok" -eq 1 ]; then
  echo "result=joystickd_residency_ok"
  exit 0
fi
echo "result=joystickd_residency_issue"
exit 1
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_JOYSTICKD_TIMEOUT_MS=$(shell_quote "$JOYSTICKD_TIMEOUT_MS")
PLUMOS_RESIDENCY_FE_TIMEOUT=$(shell_quote "$FE_TIMEOUT")
PLUMOS_RESIDENCY_SDL2_MS=$(shell_quote "$SDL2_RUN_MS")
PLUMOS_RESIDENCY_PPSSPP_MS=$(shell_quote "$PPSSPP_RUN_MS")
PLUMOS_PSP_ROM=$(shell_quote "$PPSSPP_ROM")
PLUMOS_RESIDENCY_PPSSPP=$(shell_quote "$RUN_PPSSPP")
PLUMOS_RESIDENCY_STOP_STOCK=$(shell_quote "$STOP_STOCK")
PLUMOS_FRONTEND_MALI_ROTATION=$(shell_quote "$ROTATION")
$remote_script"
