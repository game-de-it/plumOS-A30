#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
ROM="${PLUMOS_PSP_ROM:-/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-15000}"
RUN_MS="${PLUMOS_PPSSPP_RUN_MS:-3000}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD:-0}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--rom PATH] [--run-ms MS] [--timeout-ms MS]

Starts plumos-joystickd --device-mode xbox, launches stock PPSSPPSDL directly
without stock miyoo282_xpad_inputd, inspects whether PPSSPP opens the plumOS
gamepad, then stops PPSSPP.

Environment:
  A30_TARGET                  SSH target. Default: ${TARGET}
  PLUMOS_PSP_ROM              PSP ROM path. Default: ${ROM}
  PLUMOS_PPSSPP_RUN_MS        PPSSPP inspection duration. Default: ${RUN_MS}
  PLUMOS_JOYSTICKD_TIMEOUT_MS joystickd runtime. Default: ${JOYSTICKD_TIMEOUT_MS}
  PLUMOS_DEPLOY_JOYSTICKD=1   Deploy dist/plumos-joystickd before probing.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --rom)
      if [ "$#" -lt 2 ]; then
        echo "error: --rom requires a value" >&2
        exit 2
      fi
      ROM="$2"
      shift 2
      ;;
    --run-ms)
      if [ "$#" -lt 2 ]; then
        echo "error: --run-ms requires a value" >&2
        exit 2
      fi
      RUN_MS="$2"
      shift 2
      ;;
    --timeout-ms)
      if [ "$#" -lt 2 ]; then
        echo "error: --timeout-ms requires a value" >&2
        exit 2
      fi
      JOYSTICKD_TIMEOUT_MS="$2"
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

case "$RUN_MS" in
  ""|*[!0-9]*)
    echo "error: run-ms must be an integer: $RUN_MS" >&2
    exit 2
    ;;
esac
case "$JOYSTICKD_TIMEOUT_MS" in
  ""|*[!0-9]*)
    echo "error: timeout-ms must be an integer: $JOYSTICKD_TIMEOUT_MS" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

remote_script='
set -eu

ROM="${PLUMOS_PSP_ROM:?}"
RUN_MS="${PLUMOS_PPSSPP_RUN_MS:?}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:?}"
PPSSPP_DIR=/mnt/SDCARD/Emu/PPSSPP
JOY_LOG=/tmp/plumos-ppsspp-gamepad-joystickd.log
PPSSPP_LOG=/tmp/plumos-ppsspp-gamepad-ppsspp.log

[ -f "$ROM" ] || {
  echo "missing_rom=$ROM"
  exit 2
}

killall PPSSPPSDL 2>/dev/null || true
killall miyoo282_xpad_inputd 2>/dev/null || true
rm -f "$JOY_LOG" "$PPSSPP_LOG"

/mnt/SDCARD/plumos/bin/plumos-joystickd \
  --device-mode xbox \
  --timeout-ms "$JOYSTICKD_TIMEOUT_MS" \
  >"$JOY_LOG" 2>&1 &
jpid=$!
sleep 0.8

cd "$PPSSPP_DIR"
HOME=/mnt/SDCARD \
LD_LIBRARY_PATH=/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/Emu/PPSSPP:/lib:/usr/lib \
  ./PPSSPPSDL "$ROM" >"$PPSSPP_LOG" 2>&1 &
ppid=$!

sleep_sec=$((RUN_MS / 1000))
[ "$sleep_sec" -gt 0 ] || sleep_sec=1
sleep "$sleep_sec"

printf "ppsspp_pid=%s joystickd_pid=%s\n" "$ppid" "$jpid"
echo "== ppsspp fds =="
ls -l "/proc/$ppid/fd" 2>/dev/null | grep -E "input|ttyS0|PPSSPP_ID|cso|iso" || true

echo "== plumOS gamepad =="
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p"

echo "== fd holders =="
for fd in /proc/[0-9]*/fd/*; do
  target=$(readlink "$fd" 2>/dev/null) || continue
  case "$target" in
    /dev/input/*|/dev/uinput|/dev/ttyS0)
      pid=${fd#/proc/}
      pid=${pid%%/*}
      cmd=$(tr "\000" " " <"/proc/$pid/cmdline" 2>/dev/null || true)
      case "$cmd" in
        *PPSSPP*|*plumos-joystickd*|*xpad*) echo "$pid $cmd $fd -> $target" ;;
      esac
      ;;
  esac
done

kill "$ppid" 2>/dev/null || true
wait "$ppid" 2>/dev/null || true
wait "$jpid" 2>/dev/null || true

echo "== joystickd log =="
cat "$JOY_LOG"
echo "== ppsspp input log =="
grep -E "loading control pad mappings|found control pad|mapping|pad 1|setUpController|serial open|SDL version" "$PPSSPP_LOG" || true

rm -f "$JOY_LOG" "$PPSSPP_LOG"
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_PSP_ROM='$ROM'
PLUMOS_PPSSPP_RUN_MS='$RUN_MS'
PLUMOS_JOYSTICKD_TIMEOUT_MS='$JOYSTICKD_TIMEOUT_MS'
$remote_script"
