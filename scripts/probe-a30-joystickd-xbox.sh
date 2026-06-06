#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-5000}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD:-0}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--timeout-ms MS]

Creates a short-lived plumOS A30 composite gamepad with plumos-joystickd
--device-mode xbox, then verifies its js/event visibility.

Environment:
  A30_TARGET                  SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_JOYSTICKD=1   Deploy dist/plumos-joystickd before probing.
  PLUMOS_JOYSTICKD_TIMEOUT_MS Daemon runtime. Default: ${TIMEOUT_MS}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --timeout-ms)
      if [ "$#" -lt 2 ]; then
        echo "error: --timeout-ms requires a value" >&2
        exit 2
      fi
      TIMEOUT_MS="$2"
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

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

case "$TIMEOUT_MS" in
  ""|*[!0-9]*)
    echo "error: timeout must be an integer: $TIMEOUT_MS" >&2
    exit 2
    ;;
esac

remote_script='
set -eu

TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-5000}"
LOG=/tmp/plumos-joystickd-xbox.log
rm -f "$LOG"

/mnt/SDCARD/plumos/bin/plumos-joystickd \
  --device-mode xbox \
  --timeout-ms "$TIMEOUT_MS" \
  >"$LOG" 2>&1 &
jpid=$!
sleep 0.7

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

echo "detected js=$js_path event=$event_path"
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p"

if [ -n "$js_path" ] && [ -n "$event_path" ]; then
  /mnt/SDCARD/plumos/bin/plumos-joystick-reader \
    --js "$js_path" \
    --event "$event_path" \
    --name "plumOS A30 Gamepad" \
    --timeout-ms 1000
else
  echo "result=plumos_xbox_gamepad_missing"
fi

wait "$jpid" || true
echo "== joystickd log =="
cat "$LOG"
rm -f "$LOG"
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_JOYSTICKD_TIMEOUT_MS='$TIMEOUT_MS'
$remote_script"
