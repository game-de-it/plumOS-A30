#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
RUN_MS="${PLUMOS_SDL2_PROBE_MS:-5000}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-}"
DEPLOY="${PLUMOS_DEPLOY_SDL2_PROBE:-0}"
FORCE_JS_DEVICE="${PLUMOS_SDL2_FORCE_JS_DEVICE:-0}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--run-ms MS] [--timeout-ms MS] [--force-js-device]

Starts plumos-joystickd --device-mode xbox and checks whether the plumOS-bundled
SDL2 probe can see the composite gamepad through SDL2 Joystick/GameController.

Environment:
  A30_TARGET                  SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_SDL2_PROBE=1  Deploy dist/plumos-sdl2-probe and dist/plumos-joystickd.
  PLUMOS_SDL2_PROBE_MS        Probe duration. Default: ${RUN_MS}
  PLUMOS_JOYSTICKD_TIMEOUT_MS joystickd runtime. Default: run-ms + 4000
  PLUMOS_SDL2_FORCE_JS_DEVICE Set SDL_JOYSTICK_DEVICE to the detected js path.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
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
    --force-js-device)
      FORCE_JS_DEVICE=1
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

case "$RUN_MS" in
  ""|*[!0-9]*)
    echo "error: run-ms must be an integer: $RUN_MS" >&2
    exit 2
    ;;
esac

if [ -z "$JOYSTICKD_TIMEOUT_MS" ]; then
  JOYSTICKD_TIMEOUT_MS=$((RUN_MS + 4000))
fi

case "$JOYSTICKD_TIMEOUT_MS" in
  ""|*[!0-9]*)
    echo "error: timeout-ms must be an integer: $JOYSTICKD_TIMEOUT_MS" >&2
    exit 2
    ;;
esac

case "$FORCE_JS_DEVICE" in
  0|1)
    ;;
  *)
    echo "error: PLUMOS_SDL2_FORCE_JS_DEVICE must be 0 or 1: $FORCE_JS_DEVICE" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-sdl2-probe" /mnt/SDCARD
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

remote_script='
set -eu

RUN_MS="${PLUMOS_SDL2_PROBE_MS:?}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:?}"
FORCE_JS_DEVICE="${PLUMOS_SDL2_FORCE_JS_DEVICE:?}"
JOY_LOG=/tmp/plumos-sdl2-gamepad-joystickd.log
SDL_LOG=/tmp/plumos-sdl2-gamepad-probe.log
rm -f "$JOY_LOG" "$SDL_LOG"

/mnt/SDCARD/plumos/bin/plumos-joystickd \
  --device-mode xbox \
  --timeout-ms "$JOYSTICKD_TIMEOUT_MS" \
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

echo "detected js=$js_path event=$event_path"
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p"

set +e
if [ "$FORCE_JS_DEVICE" = "1" ]; then
  SDL_JOYSTICK_DEVICE="${js_path:-/dev/input/js0}" \
  SDL_VIDEODRIVER=dummy \
  SDL_AUDIODRIVER=dummy \
  PLUMOS_ROOT=/mnt/SDCARD/plumos \
    /mnt/SDCARD/plumos/bin/plumos-sdl2-probe \
      --timeout-ms "$RUN_MS" \
      >"$SDL_LOG" 2>&1
  sdl_rc=$?
else
  SDL_VIDEODRIVER=dummy \
  SDL_AUDIODRIVER=dummy \
  PLUMOS_ROOT=/mnt/SDCARD/plumos \
    /mnt/SDCARD/plumos/bin/plumos-sdl2-probe \
      --timeout-ms "$RUN_MS" \
      >"$SDL_LOG" 2>&1
  sdl_rc=$?
fi
set -e

wait "$jpid" || true

echo "== SDL2 probe =="
cat "$SDL_LOG"
echo "== joystickd log =="
cat "$JOY_LOG"
echo "sdl2_probe_rc=$sdl_rc"

if grep -q "is_controller=yes" "$SDL_LOG"; then
  echo "result=sdl2_gamecontroller_visible"
elif grep -q "joysticks=[1-9]" "$SDL_LOG"; then
  echo "result=sdl2_joystick_visible"
else
  echo "result=sdl2_gamepad_missing"
fi

rm -f "$JOY_LOG" "$SDL_LOG"
exit "$sdl_rc"
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_SDL2_PROBE_MS='$RUN_MS'
PLUMOS_JOYSTICKD_TIMEOUT_MS='$JOYSTICKD_TIMEOUT_MS'
PLUMOS_SDL2_FORCE_JS_DEVICE='$FORCE_JS_DEVICE'
$remote_script"
