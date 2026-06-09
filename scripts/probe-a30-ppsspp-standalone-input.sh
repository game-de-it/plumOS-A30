#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
ROM="${PLUMOS_PSP_ROM:-/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso}"
RUN_MS="${PLUMOS_PPSSPP_RUN_MS:-30000}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/ppsspp-standalone-input}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--rom PATH] [--run-ms MS]

Starts plumos-joystickd --device-mode xbox, launches plumOS standalone PPSSPP,
captures input/fd/log evidence, then stops all display/input test processes.
While this runs, press A/B/X/Y, D-pad, L/R, L2/R2, START/SELECT, and move the
left stick on the A30.

Defaults:
  ROM           ${ROM}
  RUN_MS        ${RUN_MS}
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
  OUT_ROOT      ${OUT_ROOT}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --rom)
      ROM="${2:?missing --rom value}"
      shift 2
      ;;
    --run-ms)
      RUN_MS="${2:?missing --run-ms value}"
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

if [ -z "$JOYSTICKD_TIMEOUT_MS" ]; then
  JOYSTICKD_TIMEOUT_MS=$((RUN_MS + 5000))
fi

case "$JOYSTICKD_TIMEOUT_MS" in
  ""|*[!0-9]*)
    echo "error: joystickd timeout must be an integer: $JOYSTICKD_TIMEOUT_MS" >&2
    exit 2
    ;;
esac

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/pre-stop.txt" || true

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "ROM='$ROM' RUN_MS='$RUN_MS' JOYSTICKD_TIMEOUT_MS='$JOYSTICKD_TIMEOUT_MS' sh -s" <<'REMOTE' \
  >"${OUT_DIR}/remote.log" 2>&1
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
ROM="${ROM:?}"
RUN_MS="${RUN_MS:?}"
JOYSTICKD_TIMEOUT_MS="${JOYSTICKD_TIMEOUT_MS:?}"
JOY_LOG=/tmp/plumos-ppsspp-standalone-joystickd.log
READER_LOG=/tmp/plumos-ppsspp-standalone-reader.log
PPSSPP_LOG=/tmp/plumos-ppsspp-standalone.log
PIDFILE=/tmp/plumos-ppsspp-standalone.pid

cleanup() {
  [ -n "${reader_pid:-}" ] && kill "$reader_pid" 2>/dev/null || true
  [ -n "${ppid:-}" ] && kill "$ppid" 2>/dev/null || true
  [ -n "${jpid:-}" ] && kill "$jpid" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

echo "rom=$ROM"
echo "run_ms=$RUN_MS"
echo "joystickd_timeout_ms=$JOYSTICKD_TIMEOUT_MS"

[ -f "$ROM" ] || {
  echo "result=missing_rom"
  exit 2
}

rm -f "$JOY_LOG" "$READER_LOG" "$PPSSPP_LOG" "$PIDFILE"

"$PLUMOS_ROOT/bin/plumos-fb-restore" --quiet >/dev/null 2>&1 || true

"$PLUMOS_ROOT/bin/plumos-joystickd" \
  --device-mode xbox \
  --timeout-ms "$JOYSTICKD_TIMEOUT_MS" \
  --print-every 120 \
  >"$JOY_LOG" 2>&1 &
jpid=$!
sleep 0.8

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
echo "detected_gamepad js=$js_path event=$event_path"
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p" || true

if [ -n "$js_path" ] && [ -n "$event_path" ]; then
  "$PLUMOS_ROOT/bin/plumos-joystick-reader" \
    --js "$js_path" \
    --event "$event_path" \
    --name "plumOS A30 Gamepad" \
    --timeout-ms "$RUN_MS" \
    >"$READER_LOG" 2>&1 &
  reader_pid=$!
else
  echo "result=gamepad_missing" >"$READER_LOG"
  reader_pid=""
fi

start-stop-daemon -S -b -m -p "$PIDFILE" -x /bin/sh -- -c \
  "exec env PLUMOS_STANDALONE_USE_STOCK_SDL=1 '$PLUMOS_ROOT/bin/plumos-standalone-launch' ppsspp '$ROM' >'$PPSSPP_LOG' 2>&1 < /dev/null"
ppid=$(cat "$PIDFILE" 2>/dev/null || true)
echo "ppsspp_wrapper_pid=$ppid"

sleep_sec=$((RUN_MS / 1000))
[ "$sleep_sec" -gt 0 ] || sleep_sec=1
sleep "$sleep_sec"

echo "== PPSSPP/input fd holders during run =="
for fd in /proc/[0-9]*/fd/*; do
  target=$(readlink "$fd" 2>/dev/null) || continue
  case "$target" in
    /dev/input/*|/dev/uinput|/dev/ttyS0|/dev/fb0)
      pid=${fd#/proc/}
      pid=${pid%%/*}
      cmd=$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)
      comm=$(cat "/proc/$pid/comm" 2>/dev/null || true)
      case "$cmd $comm" in
        *plumos*|*PPSSPP*|*ld-linux-armhf*|*xpad*|*MainUI*|*keymon*)
          echo "$pid $comm $cmd $fd -> $target"
          ;;
      esac
      ;;
  esac
done | sort

if [ -n "${reader_pid:-}" ]; then
  wait "$reader_pid" 2>/dev/null || true
fi

if [ -n "$ppid" ]; then
  start-stop-daemon -K -p "$PIDFILE" -s TERM 2>/dev/null || true
fi
sleep 1

echo "== joystickd log =="
cat "$JOY_LOG" || true
echo "== reader log =="
cat "$READER_LOG" || true
echo "== ppsspp log =="
grep -E "SDL version|loading control pad mappings|found control pad|pad 1|A30 display|BOOT|underrun|Leaving main|CreateWindow failed" "$PPSSPP_LOG" || true

if grep -q "button_events=[1-9]" "$JOY_LOG"; then
  echo "result=button_forwarding_observed"
elif grep -q "type_name=EV_KEY\|type_name=EV_ABS\|type_name=JS_BUTTON\|type_name=JS_AXIS" "$READER_LOG"; then
  echo "result=virtual_input_events_observed"
else
  echo "result=no_physical_input_events_observed"
fi

rm -f "$JOY_LOG" "$READER_LOG" "$PPSSPP_LOG" "$PIDFILE"
REMOTE

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/post-stop.txt" || true

printf 'out_dir=%s\n' "$OUT_DIR"
cat "${OUT_DIR}/remote.log"
cat "${OUT_DIR}/post-stop.txt"
