#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
MODE="${1:-layout-portrait-vertical}"
ROM="${2:-/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso}"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-captures/ppsspp-orientation}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [MODE] [ROM]

MODE:
  default
  layout-portrait-vertical
  layout-portrait-vertical180
  patch-ccw
  patch-ccw-swap
  patch-cw
  patch-cw-swap

Defaults:
  MODE          layout-portrait-vertical
  ROM           /mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso
  OUT_ROOT      ${OUT_ROOT}
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

case "$MODE" in
  default|layout-portrait-vertical|layout-portrait-vertical180|patch-ccw|patch-ccw-swap|patch-cw|patch-cw-swap)
    ;;
  *)
    echo "error: unknown mode: $MODE" >&2
    usage >&2
    exit 2
    ;;
esac

if ! command -v magick >/dev/null 2>&1; then
  echo "error: ImageMagick 'magick' is required" >&2
  exit 127
fi

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}-${MODE}"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >/dev/null

ssh "${SSH_OPTS[@]}" "$TARGET" "MODE='$MODE' ROM='$ROM' sh -s" <<'REMOTE'
set -eu

mkdir -p /mnt/SDCARD/plumos/logs/standalone /tmp
/mnt/SDCARD/plumos/bin/plumos-fb-restore --quiet >/dev/null 2>&1 || fbset -g 480 640 480 1280 32 >/dev/null 2>&1 || true
dd if=/dev/zero of=/dev/fb0 bs=2457600 count=1 >/dev/null 2>&1 || true

cfg=/tmp/plumos-ppsspp-a30-orientation.ini
args=""
rotation=none
swap=0

case "$MODE" in
  default)
    rm -f "$cfg"
    ;;
  layout-portrait-vertical)
    cat >"$cfg" <<EOF
[DisplayLayout.Portrait]
InternalScreenRotation = 2
RotateControlsWithScreen = True
DisplayStretch = False
DisplayOffsetX = 0.500000
DisplayOffsetY = 0.500000
DisplayScale = 1.000000
DisplayIntegerScale = False
DisplayAspectRatio = 1.000000
IgnoreScreenInsets = True
EOF
    args="--appendconfig=$cfg"
    ;;
  layout-portrait-vertical180)
    cat >"$cfg" <<EOF
[DisplayLayout.Portrait]
InternalScreenRotation = 4
RotateControlsWithScreen = True
DisplayStretch = False
DisplayOffsetX = 0.500000
DisplayOffsetY = 0.500000
DisplayScale = 1.000000
DisplayIntegerScale = False
DisplayAspectRatio = 1.000000
IgnoreScreenInsets = True
EOF
    args="--appendconfig=$cfg"
    ;;
  patch-ccw)
    rotation=ccw
    ;;
  patch-ccw-swap)
    rotation=ccw
    swap=1
    ;;
  patch-cw)
    rotation=cw
    ;;
  patch-cw-swap)
    rotation=cw
    swap=1
    ;;
esac

pid=/tmp/plumos-ppsspp-orientation.pid
log="/mnt/SDCARD/plumos/logs/standalone/ppsspp-orientation-${MODE}.log"
rm -f "$pid" "$log"
start-stop-daemon -S -b -m -p "$pid" -x /bin/sh -- -c \
  "exec env PLUMOS_A30_DISPLAY_ROTATION=$rotation PLUMOS_A30_DISPLAY_SWAP=$swap PLUMOS_STANDALONE_USE_STOCK_SDL=1 /mnt/SDCARD/plumos/bin/plumos-standalone-launch ppsspp $args '$ROM' >'$log' 2>&1 < /dev/null"

echo "mode=$MODE"
echo "pid=$(cat "$pid" 2>/dev/null || true)"
echo "log=$log"
echo "rom=$ROM"
echo "rotation=$rotation"
echo "swap=$swap"
if [ -f "$cfg" ]; then
  cat "$cfg"
fi
REMOTE

sleep 1.5
for frame in 01 02 03 04; do
  frame_dir="${OUT_DIR}/frame-${frame}"
  "${ROOT_DIR}/scripts/capture-a30-framebuffer.sh" "$frame_dir" >"${frame_dir}.capture.txt"
  cat "${frame_dir}.capture.txt"
  sleep 1
done

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/stop.txt" || true
cat "${OUT_DIR}/stop.txt"

ssh "${SSH_OPTS[@]}" "$TARGET" "tail -n 180 '/mnt/SDCARD/plumos/logs/standalone/ppsspp-orientation-${MODE}.log' 2>/dev/null || true" >"${OUT_DIR}/ppsspp.log"

visible_files=()
while IFS= read -r file; do
  visible_files+=("$file")
done < <(find "$OUT_DIR" -path '*/frame-*/*.visible.png' | sort)

cw_files=()
while IFS= read -r file; do
  cw_files+=("$file")
done < <(find "$OUT_DIR" -path '*/frame-*/*.visible.cw.png' | sort)

if [ "${#visible_files[@]}" -gt 0 ]; then
  magick "${visible_files[@]}" -resize 240x320 +append "${OUT_DIR}/contact-visible.png"
fi
if [ "${#cw_files[@]}" -gt 0 ]; then
  magick "${cw_files[@]}" -resize 320x240 +append "${OUT_DIR}/contact-cw.png"
fi

printf 'out_dir=%s\n' "$OUT_DIR"
find "$OUT_DIR" -maxdepth 2 -type f \( -name 'contact-*.png' -o -name '*.log' -o -name 'stop.txt' \) -print | sort
