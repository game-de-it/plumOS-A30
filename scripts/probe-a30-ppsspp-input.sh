#!/usr/bin/env bash
set -euo pipefail

TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
READER_TIMEOUT_MS="${PLUMOS_READER_TIMEOUT_MS:-1000}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0

Inspects the stock PPSSPP analog-stick input path while PPSSPP is running.

Environment:
  A30_TARGET                 SSH target. Default: ${TARGET}
  A30_SSH_PORT               SSH port. Default: ${PORT}
  PLUMOS_READER_TIMEOUT_MS   joystick-reader duration. Default: ${READER_TIMEOUT_MS}
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

remote_script='
set -eu

echo "== ppsspp processes =="
ps | grep -E "PPSSPP|xpad|keymon" | grep -v grep || true
echo

echo "== launch script =="
sed -n "1,140p" /mnt/SDCARD/Emu/PPSSPP/launch.sh 2>/dev/null || true
echo

echo "== input devices =="
cat /proc/bus/input/devices
echo

echo "== miyoo pad sysfs =="
for d in /sys/class/input/event*/device; do
  name=$(cat "$d/name" 2>/dev/null || true)
  case "$name" in
    MIYOO\ Pad*)
      echo "base=$d"
      for f in name id/bustype id/vendor id/product id/version capabilities/ev capabilities/key capabilities/abs capabilities/sw properties; do
        [ -r "$d/$f" ] && echo "$f=$(cat "$d/$f")"
      done
      ;;
  esac
done
echo

echo "== input fd holders =="
for fd in /proc/[0-9]*/fd/*; do
  target=$(readlink "$fd" 2>/dev/null) || continue
  case "$target" in
    /dev/input/*|/dev/uinput|/dev/ttyS0)
      pid=${fd#/proc/}
      pid=${pid%%/*}
      cmd=$(tr "\000" " " <"/proc/$pid/cmdline" 2>/dev/null || true)
      case "$cmd" in
        *PPSSPP*|*xpad*|*keymon*)
          echo "$pid $cmd $fd -> $target"
          ;;
      esac
      ;;
  esac
done
echo

echo "== ppsspp process details =="
for p in /proc/[0-9]*; do
  cmd=$(tr "\000" " " <"$p/cmdline" 2>/dev/null || true)
  case "$cmd" in
    *PPSSPP*|*xpad*)
      pid=${p#/proc/}
      echo "pid=$pid cmd=$cmd"
      echo "fds:"
      ls -l "$p/fd" 2>/dev/null | grep -E "input|uinput|ttyS0|PPSSPP|cso|iso" || true
      echo "maps:"
      grep -E "SDL|PPSSPP|xpad|libstdc" "$p/maps" 2>/dev/null | sed -n "1,120p" || true
      echo
      ;;
  esac
done

echo "== joystick reader =="
if [ -x /mnt/SDCARD/plumos/bin/plumos-joystick-reader ] && [ -e /dev/input/js0 ]; then
  /mnt/SDCARD/plumos/bin/plumos-joystick-reader \
    --js /dev/input/js0 \
    --event /dev/input/event4 \
    --timeout-ms "$PLUMOS_READER_TIMEOUT_MS" || true
else
  echo "plumos-joystick-reader or /dev/input/js0 is not available"
fi
echo

echo "== calibration =="
cat /config/joypad.config 2>/dev/null || true
echo
'

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "PLUMOS_READER_TIMEOUT_MS='$READER_TIMEOUT_MS' sh -s" <<<"$remote_script"

if command -v strings >/dev/null 2>&1; then
  echo "== miyoo282_xpad_inputd strings =="
  ssh "${SSH_OPTS[@]}" "$TARGET" 'cat /mnt/SDCARD/Emu/PPSSPP/miyoo282_xpad_inputd 2>/dev/null' \
    | strings \
    | grep -E 'MIYOO|ttyS0|uinput|joypad|ABS|cal|Pad' \
    | sed -n '1,120p' || true
  echo

  echo "== PPSSPPSDL SDL strings =="
  ssh "${SSH_OPTS[@]}" "$TARGET" 'cat /mnt/SDCARD/Emu/PPSSPP/PPSSPPSDL 2>/dev/null' \
    | strings \
    | grep -E 'SDL_GameController|SDL_Joystick|libSDL2|AnalogSetup|RightAnalogPress|SDL/SDLMain.cpp|UI/ControlMappingScreen.cpp' \
    | sed -n '1,120p' || true
  echo
else
  echo "warning: local strings command is not available; skipped binary string scan" >&2
fi

echo "== PPSSPP gamecontrollerdb Xbox entries =="
ssh "${SSH_OPTS[@]}" "$TARGET" \
  'grep -n "045e.*028e\|Xbox 360" /mnt/SDCARD/Emu/PPSSPP/assets/gamecontrollerdb.txt 2>/dev/null | head -20' || true
