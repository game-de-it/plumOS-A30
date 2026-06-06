#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_JOYSTICKD:-0}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [--deploy]

Runs a short RetroArch SDL joystick visibility probe on the A30.

Environment:
  A30_TARGET                 SSH target. Default: ${TARGET}
  PLUMOS_DEPLOY_JOYSTICKD=1  Deploy dist/plumos-joystickd before probing.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
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

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/autoconfig-sdl" "$TMP_DIR/autoconfig-sdl-dingux"

cat >"$TMP_DIR/autoconfig-sdl/plumos-a30-analog-stick.cfg" <<EOF
input_driver = "sdl"
input_device = "plumOS A30 Analog Stick"
input_device_display_name = "plumOS A30 Analog Stick"
input_l_x_minus_axis = "-0"
input_l_x_plus_axis = "+0"
input_l_y_minus_axis = "-1"
input_l_y_plus_axis = "+1"
EOF

cat >"$TMP_DIR/autoconfig-sdl-dingux/plumos-a30-analog-stick.cfg" <<EOF
input_driver = "sdl_dingux"
input_device = "plumOS A30 Analog Stick"
input_device_display_name = "plumOS A30 Analog Stick"
input_l_x_minus_axis = "-0"
input_l_x_plus_axis = "+0"
input_l_y_minus_axis = "-1"
input_l_y_plus_axis = "+1"
EOF

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

  "$PLUMOS_BIN/plumos-joystickd" --timeout-ms 9000 >"$case_dir/joystickd.log" 2>&1 &
  jpid=$!
  sleep 0.5

  "$PLUMOS_BIN/plumos-joystick-reader" --timeout-ms 500 >"$case_dir/reader.log" 2>&1 || true

  cd "$RA_DIR"
  SDL_JOYSTICK_DEVICE=/dev/input/js0 \
  HOME="$RA_DIR" \
  LD_LIBRARY_PATH=/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/RetroArch \
    "$RA_BIN" -v --menu --max-frames 60 \
    --config "$RA_CFG" \
    --appendconfig "$case_dir/append.cfg" \
    --log-file "$case_dir/retroarch.log" \
    >"$case_dir/retroarch.stdout" 2>&1
  rc=$?

  wait "$jpid" || true

  echo "== case: $label =="
  echo "retroarch_rc=$rc"
  echo "-- reader --"
  sed -n "1,80p" "$case_dir/reader.log"
  echo "-- retroarch input log --"
  grep -i "joy\|sdl\|input\|autoconfig\|plumos\|analog\|event\|js\|configured\|device" \
    "$case_dir/retroarch.log" | sed -n "1,220p" || true
  echo "-- joystickd --"
  sed -n "1,80p" "$case_dir/joystickd.log"

  if grep -qi "plumOS A30 Analog Stick.*configured\|configured.*plumOS A30 Analog Stick" \
      "$case_dir/retroarch.log"; then
    echo "result=retroarch_autoconfigured"
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

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" "$remote_script"
