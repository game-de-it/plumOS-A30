#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_MALI_EGL_PROBE:-0}"
RUN_MS="${PLUMOS_MALI_EGL_PROBE_MS:-500}"
FRAMES="${PLUMOS_MALI_EGL_PROBE_FRAMES:-30}"
WINDOW_MODE="${PLUMOS_MALI_EGL_WINDOW_MODE:-auto}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--deploy] [--run-ms MS] [--frames N] [--window-mode auto|u16|u32|null]

Runs the plumOS fbdev + Mali EGL/GLES probe on the A30. The probe dlopens the
device rootfs EGL/GLES libraries and does not link to stock SDL.

Environment:
  PLUMOS_DEPLOY_MALI_EGL_PROBE Deploy dist/plumos-mali-egl-probe first. Default: ${DEPLOY}
  PLUMOS_MALI_EGL_PROBE_MS     Draw/swap duration. Default: ${RUN_MS}
  PLUMOS_MALI_EGL_PROBE_FRAMES Max frames to draw. Default: ${FRAMES}
  PLUMOS_MALI_EGL_WINDOW_MODE  auto, u16, u32, null. Default: ${WINDOW_MODE}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --run-ms)
      RUN_MS="${2:?missing --run-ms value}"
      shift 2
      ;;
    --frames)
      FRAMES="${2:?missing --frames value}"
      shift 2
      ;;
    --window-mode)
      WINDOW_MODE="${2:?missing --window-mode value}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

case "$DEPLOY" in
  0|1) ;;
  *)
    echo "error: PLUMOS_DEPLOY_MALI_EGL_PROBE must be 0 or 1: $DEPLOY" >&2
    exit 2
    ;;
esac

case "$RUN_MS" in
  ''|*[!0-9]*)
    echo "error: --run-ms must be a non-negative integer: $RUN_MS" >&2
    exit 2
    ;;
esac

case "$FRAMES" in
  ''|*[!0-9]*)
    echo "error: --frames must be a non-negative integer: $FRAMES" >&2
    exit 2
    ;;
esac

case "$WINDOW_MODE" in
  auto|u16|u32|null) ;;
  *)
    echo "error: --window-mode must be auto, u16, u32, or null: $WINDOW_MODE" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-mali-egl-probe" /mnt/SDCARD
fi

A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/run-a30.sh" \
  "PLUMOS_MALI_EGL_PROBE_MS='$RUN_MS'
PLUMOS_MALI_EGL_PROBE_FRAMES='$FRAMES'
PLUMOS_MALI_EGL_WINDOW_MODE='$WINDOW_MODE'
set -eu

PROBE=/mnt/SDCARD/plumos/bin/plumos-mali-egl-probe

echo '== device inventory =='
ls -l /dev/fb* /dev/mali /dev/disp /dev/ion 2>/dev/null || true
echo '== proc fb =='
cat /proc/fb 2>/dev/null || true
echo '== gpu libraries =='
ls -l /usr/lib/libEGL.so /usr/lib/libGLESv2.so /usr/lib/libMali.so /usr/lib/libUMP.so.3.0.0 2>/dev/null || true
echo '== running display processes =='
ps w 2>/dev/null | grep -E 'MainUI|retroarch|PPSSPP|plumos-mali-egl-probe' | grep -v grep || true
echo '== probe =='
PLUMOS_ROOT=/mnt/SDCARD/plumos \
  \"\$PROBE\" \
  --run-ms \"\$PLUMOS_MALI_EGL_PROBE_MS\" \
  --frames \"\$PLUMOS_MALI_EGL_PROBE_FRAMES\" \
  --window-mode \"\$PLUMOS_MALI_EGL_WINDOW_MODE\"
"
