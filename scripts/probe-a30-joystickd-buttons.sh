#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
READER_MS="${PLUMOS_JOYSTICK_READER_MS:-20000}"
DAEMON_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-}"
TRIGGER_MODE="${PLUMOS_JOYSTICKD_TRIGGER_MODE:-buttons}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD:-0}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--reader-ms MS] [--timeout-ms MS] [--trigger-mode buttons|axes]

Creates a short-lived plumOS A30 composite gamepad and watches forwarded
button/hat/trigger events. While this runs, press A/B/X/Y, D-pad, L/R, L2/R2,
START/SELECT, and Function on the A30.

Environment:
  A30_TARGET                  SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_JOYSTICKD=1   Deploy dist/plumos-joystickd before probing.
  PLUMOS_JOYSTICK_READER_MS   Reader duration. Default: ${READER_MS}
  PLUMOS_JOYSTICKD_TIMEOUT_MS Daemon runtime. Default: reader-ms + 3000
  PLUMOS_JOYSTICKD_TRIGGER_MODE
                              L2/R2 output mode. Default: ${TRIGGER_MODE}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --reader-ms)
      if [ "$#" -lt 2 ]; then
        echo "error: --reader-ms requires a value" >&2
        exit 2
      fi
      READER_MS="$2"
      shift 2
      ;;
    --timeout-ms)
      if [ "$#" -lt 2 ]; then
        echo "error: --timeout-ms requires a value" >&2
        exit 2
      fi
      DAEMON_MS="$2"
      shift 2
      ;;
    --trigger-mode)
      if [ "$#" -lt 2 ]; then
        echo "error: --trigger-mode requires a value" >&2
        exit 2
      fi
      TRIGGER_MODE="$2"
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

case "$READER_MS" in
  ""|*[!0-9]*)
    echo "error: reader-ms must be an integer: $READER_MS" >&2
    exit 2
    ;;
esac

if [ -z "$DAEMON_MS" ]; then
  DAEMON_MS=$((READER_MS + 3000))
fi

case "$DAEMON_MS" in
  ""|*[!0-9]*)
    echo "error: timeout-ms must be an integer: $DAEMON_MS" >&2
    exit 2
    ;;
esac

case "$TRIGGER_MODE" in
  buttons|button|btn) TRIGGER_MODE=buttons ;;
  axes|axis|analog) TRIGGER_MODE=axes ;;
  *)
    echo "error: trigger-mode must be buttons or axes: $TRIGGER_MODE" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

remote_script='
set -eu

READER_MS="${PLUMOS_JOYSTICK_READER_MS:?}"
DAEMON_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:?}"
TRIGGER_MODE="${PLUMOS_JOYSTICKD_TRIGGER_MODE:?}"
JOY_LOG=/tmp/plumos-joystickd-buttons.log
READER_LOG=/tmp/plumos-joystickd-button-reader.log
rm -f "$JOY_LOG" "$READER_LOG"

echo "watching_ms=$READER_MS"
echo "expected:"
echo "  A/B/X/Y -> BTN_A/BTN_B/BTN_X/BTN_Y"
echo "  D-pad   -> ABS_HAT0X/ABS_HAT0Y"
echo "  L/R     -> BTN_TL/BTN_TR"
if [ "$TRIGGER_MODE" = "buttons" ]; then
  echo "  L2/R2   -> BTN_TL2/BTN_TR2"
else
  echo "  L2/R2   -> ABS_Z/ABS_RZ"
fi
echo "  START   -> BTN_START"
echo "  SELECT  -> BTN_SELECT"
echo "  FUNC    -> BTN_MODE"

/mnt/SDCARD/plumos/bin/plumos-joystickd \
  --device-mode xbox \
  --trigger-mode "$TRIGGER_MODE" \
  --timeout-ms "$DAEMON_MS" \
  >"$JOY_LOG" 2>&1 &
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
    --timeout-ms "$READER_MS" \
    >"$READER_LOG" 2>&1 || true
else
  echo "result=plumos_xbox_gamepad_missing" >"$READER_LOG"
fi

wait "$jpid" || true

echo "== reader events =="
cat "$READER_LOG"
echo "== event summary =="
grep -E "type_name=EV_KEY|type_name=EV_ABS|type_name=JS_BUTTON number|type_name=JS_AXIS number" "$READER_LOG" | \
  sed "s/^/  /" || true
echo "== joystickd log =="
cat "$JOY_LOG"

if grep -q "button_events=[1-9]" "$JOY_LOG"; then
  echo "result=button_forwarding_observed"
elif grep -q "type_name=EV_KEY\|type_name=EV_ABS" "$READER_LOG"; then
  echo "result=virtual_events_observed"
else
  echo "result=no_button_events_observed"
fi

rm -f "$JOY_LOG" "$READER_LOG"
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_JOYSTICK_READER_MS='$READER_MS'
PLUMOS_JOYSTICKD_TIMEOUT_MS='$DAEMON_MS'
PLUMOS_JOYSTICKD_TRIGGER_MODE='$TRIGGER_MODE'
$remote_script"
