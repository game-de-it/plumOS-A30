#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD:-0}"
DEVICE_MODE="${PLUMOS_RA_JOYSTICK_MODE:-analog}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:-9000}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy] [--device-mode analog|xbox] [--timeout-ms MS]

Runs a short RetroArch SDL joystick visibility probe on the A30.

Environment:
  A30_TARGET                  SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_JOYSTICKD=1   Deploy dist/plumos-joystickd before probing.
  PLUMOS_RA_JOYSTICK_MODE     analog or xbox. Default: ${DEVICE_MODE}
  PLUMOS_JOYSTICKD_TIMEOUT_MS Daemon runtime. Default: ${JOYSTICKD_TIMEOUT_MS}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --device-mode)
      if [ "$#" -lt 2 ]; then
        echo "error: --device-mode requires a value" >&2
        exit 2
      fi
      DEVICE_MODE="$2"
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

case "$DEVICE_MODE" in
  analog|xbox) ;;
  *)
    echo "error: --device-mode must be analog or xbox: $DEVICE_MODE" >&2
    exit 2
    ;;
esac

case "$JOYSTICKD_TIMEOUT_MS" in
  ""|*[!0-9]*)
    echo "error: timeout must be an integer: $JOYSTICKD_TIMEOUT_MS" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-joystickd" /mnt/SDCARD
fi

remote_script='
set -eu

RA_DIR=/mnt/SDCARD/RetroArch
RA_BIN="$RA_DIR/retroarch"
RA_CFG="$RA_DIR/retroarch.cfg"
PLUMOS_BIN=/mnt/SDCARD/plumos/bin
TMP_DIR=/tmp/plumos-ra-joystick-probe
DEVICE_MODE="${PLUMOS_RA_JOYSTICK_MODE:?}"
JOYSTICKD_TIMEOUT_MS="${PLUMOS_JOYSTICKD_TIMEOUT_MS:?}"

case "$DEVICE_MODE" in
  xbox)
    DEVICE_NAME="plumOS A30 Gamepad"
    DEVICE_SLUG="plumos-a30-gamepad"
    JOYSTICKD_ARGS="--device-mode xbox"
    ;;
  *)
    DEVICE_NAME="plumOS A30 Analog Stick"
    DEVICE_SLUG="plumos-a30-analog-stick"
    JOYSTICKD_ARGS="--device-mode analog"
    ;;
esac

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/autoconfig-sdl" "$TMP_DIR/autoconfig-sdl-dingux"

write_autoconfig() {
  out="$1"
  driver="$2"
  device="$3"

  cat >"$out" <<EOF
input_driver = "$driver"
input_device = "$device"
input_device_display_name = "$DEVICE_NAME"
input_b_btn = "0"
input_a_btn = "1"
input_y_btn = "2"
input_x_btn = "3"
input_l_btn = "4"
input_r_btn = "5"
input_select_btn = "6"
input_start_btn = "7"
input_l_x_minus_axis = "-0"
input_l_x_plus_axis = "+0"
input_l_y_minus_axis = "-1"
input_l_y_plus_axis = "+1"
input_l2_axis = "+2"
input_r2_axis = "+5"
input_left_axis = "-6"
input_right_axis = "+6"
input_up_axis = "-7"
input_down_axis = "+7"
EOF
}

write_autoconfig "$TMP_DIR/autoconfig-sdl/$DEVICE_SLUG.cfg" "sdl" "$DEVICE_NAME"
write_autoconfig "$TMP_DIR/autoconfig-sdl/${DEVICE_SLUG}-xbox-alias.cfg" "sdl" "Atari Xbox 360 Game Controller"
write_autoconfig "$TMP_DIR/autoconfig-sdl-dingux/$DEVICE_SLUG.cfg" "sdl_dingux" "$DEVICE_NAME"
write_autoconfig "$TMP_DIR/autoconfig-sdl-dingux/${DEVICE_SLUG}-xbox-alias.cfg" "sdl_dingux" "Atari Xbox 360 Game Controller"

run_case() {
  label="$1"
  autoconfig_dir="$2"
  case_dir="$TMP_DIR/$label"
  mkdir -p "$case_dir"

  cat >"$case_dir/append.cfg" <<EOF
input_driver = "sdl"
input_joypad_driver = "sdl"
input_autodetect_enable = "true"
notification_show_autoconfig = "true"
joypad_autoconfig_dir = "$autoconfig_dir"
input_player1_joypad_index = "0"
audio_driver = "null"
EOF

  "$PLUMOS_BIN/plumos-joystickd" $JOYSTICKD_ARGS --timeout-ms "$JOYSTICKD_TIMEOUT_MS" >"$case_dir/joystickd.log" 2>&1 &
  jpid=$!
  sleep 0.5

  event_path=""
  js_path=""
  for e in /sys/class/input/event*; do
    [ -r "$e/device/name" ] || continue
    [ "$(cat "$e/device/name")" = "$DEVICE_NAME" ] || continue
    event_path="/dev/input/${e##*/}"
  done
  for j in /sys/class/input/js*; do
    [ -r "$j/device/name" ] || continue
    [ "$(cat "$j/device/name")" = "$DEVICE_NAME" ] || continue
    js_path="/dev/input/${j##*/}"
  done

  echo "detected mode=$DEVICE_MODE name=\"$DEVICE_NAME\" js=$js_path event=$event_path" >"$case_dir/reader.log"
  if [ -n "$js_path" ] && [ -n "$event_path" ]; then
    "$PLUMOS_BIN/plumos-joystick-reader" \
      --js "$js_path" \
      --event "$event_path" \
      --name "$DEVICE_NAME" \
      --timeout-ms 500 >>"$case_dir/reader.log" 2>&1 || true
  else
    "$PLUMOS_BIN/plumos-joystick-reader" \
      --name "$DEVICE_NAME" \
      --timeout-ms 500 >>"$case_dir/reader.log" 2>&1 || true
    [ -n "$js_path" ] || js_path=/dev/input/js0
  fi

  cd "$RA_DIR"
  set +e
  SDL_JOYSTICK_DEVICE="$js_path" \
  HOME="$RA_DIR" \
  LD_LIBRARY_PATH=/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/RetroArch \
    "$RA_BIN" -v --menu --max-frames 60 \
    --config "$RA_CFG" \
    --appendconfig "$case_dir/append.cfg" \
    --log-file "$case_dir/retroarch.log" \
    >"$case_dir/retroarch.stdout" 2>&1
  rc=$?
  set -e

  wait "$jpid" || true

  echo "== case: $label =="
  echo "retroarch_rc=$rc"
  echo "-- reader --"
  sed -n "1,80p" "$case_dir/reader.log"
  echo "-- retroarch input log --"
  grep -i "joy\|sdl\|input\|autoconfig\|plumos\|analog\|event\|js\|configured\|device" \
    "$case_dir/retroarch.log" | sed -n "1,220p" || true
  echo "-- retroarch stdout --"
  grep -i "joy\|sdl\|input\|autoconfig\|plumos\|analog\|event\|js\|configured\|device" \
    "$case_dir/retroarch.stdout" | sed -n "1,120p" || true
  echo "-- joystickd --"
  sed -n "1,80p" "$case_dir/joystickd.log"

  if grep -qi "$DEVICE_NAME.*configured\|configured.*$DEVICE_NAME\|Atari Xbox 360 Game Controller.*configured\|configured.*Atari Xbox 360 Game Controller" \
      "$case_dir/retroarch.log"; then
    echo "result=retroarch_autoconfigured"
  elif grep -qi "$DEVICE_NAME\|Atari Xbox 360 Game Controller" "$case_dir/retroarch.log"; then
    echo "result=retroarch_device_logged"
  elif grep -qi "Found joypad driver: \"sdl\"" "$case_dir/retroarch.log"; then
    echo "result=retroarch_sdl_driver_only"
  else
    echo "result=retroarch_no_sdl_joypad_driver"
  fi
}

run_case sdl "$TMP_DIR/autoconfig-sdl"
run_case sdl_dingux "$TMP_DIR/autoconfig-sdl-dingux"

rm -rf "$TMP_DIR"
'

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" \
  "PLUMOS_RA_JOYSTICK_MODE='$DEVICE_MODE'
PLUMOS_JOYSTICKD_TIMEOUT_MS='$JOYSTICKD_TIMEOUT_MS'
$remote_script"
