#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_FRONTEND:-0}"
TIMEOUT="${PLUMOS_FRONTEND_MALI_TIMEOUT:-3}"
SCRIPT="${PLUMOS_FRONTEND_MALI_SCRIPT:-}"
EXERCISE="${PLUMOS_FRONTEND_MALI_EXERCISE:-0}"
RUN_SCAN="${PLUMOS_FRONTEND_MALI_SCAN:-1}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--deploy] [--timeout SEC] [--script TOKENS] [--exercise N] [--no-scan]

Runs the plumOS controller UI through the fbdev + Mali EGL renderer on the A30.
The binary dlopens /usr/lib/libEGL.so and /usr/lib/libGLESv2.so and does not
link to stock SDL.

Environment:
  PLUMOS_DEPLOY_FRONTEND       Deploy dist/plumos-frontend first. Default: ${DEPLOY}
  PLUMOS_FRONTEND_MALI_TIMEOUT Display/event-loop duration in seconds. Default: ${TIMEOUT}
  PLUMOS_FRONTEND_MALI_SCRIPT  Optional controller script, e.g. down,a,b,q.
  PLUMOS_FRONTEND_MALI_EXERCISE Generate a repeated navigation script. Default: ${EXERCISE}
  PLUMOS_FRONTEND_MALI_SCAN    Run plumos-library-scan first. Default: ${RUN_SCAN}
EOF
}

shell_quote() {
  local s="$1"
  printf "'%s'" "${s//\'/\'\\\'\'}"
}

make_exercise_script() {
  local count="$1"
  local base="down,a,b,up,a,b,start,a,b,function,b"
  local out=""
  local i

  i=0
  while [ "$i" -lt "$count" ]; do
    if [ -n "$out" ]; then
      out="${out},${base}"
    else
      out="$base"
    fi
    i=$((i + 1))
  done
  printf '%s' "$out"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --timeout)
      TIMEOUT="${2:?missing --timeout value}"
      shift 2
      ;;
    --script)
      SCRIPT="${2:?missing --script value}"
      shift 2
      ;;
    --exercise)
      EXERCISE="${2:?missing --exercise value}"
      shift 2
      ;;
    --no-scan)
      RUN_SCAN=0
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

case "$DEPLOY" in
  0|1) ;;
  *)
    echo "error: PLUMOS_DEPLOY_FRONTEND must be 0 or 1: $DEPLOY" >&2
    exit 2
    ;;
esac

case "$RUN_SCAN" in
  0|1) ;;
  *)
    echo "error: PLUMOS_FRONTEND_MALI_SCAN must be 0 or 1: $RUN_SCAN" >&2
    exit 2
    ;;
esac

case "$TIMEOUT" in
  ''|*[!0-9]*)
    echo "error: --timeout must be a non-negative integer: $TIMEOUT" >&2
    exit 2
    ;;
esac

case "$EXERCISE" in
  ''|*[!0-9]*)
    echo "error: --exercise must be a non-negative integer: $EXERCISE" >&2
    exit 2
    ;;
esac

case "$SCRIPT" in
  *[!abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_,:-]*)
    echo "error: --script contains unsupported characters: $SCRIPT" >&2
    exit 2
    ;;
esac

if [ "$EXERCISE" -gt 0 ] && [ -z "$SCRIPT" ]; then
  SCRIPT="$(make_exercise_script "$EXERCISE")"
fi

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-frontend" /mnt/SDCARD
fi

remote_script='
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
export PLUMOS_ROOT
export PLUMOS_SDCARD_ROOT=/mnt/SDCARD
UI="$PLUMOS_ROOT/bin/plumos-controller-ui-mali"
SCAN="$PLUMOS_ROOT/bin/plumos-library-scan"
SCAN_LOG=/tmp/plumos-frontend-mali-scan.log

echo "== device inventory =="
ls -l /dev/fb* /dev/mali /dev/disp /dev/ion 2>/dev/null || true
echo "== gpu libraries =="
ls -l /usr/lib/libEGL.so /usr/lib/libGLESv2.so /usr/lib/libMali.so 2>/dev/null || true
echo "== frontend binaries =="
ls -l "$UI" "$PLUMOS_ROOT/bin/plumos-controller-ui-mali.bin" "$SCAN" 2>/dev/null || true
echo "== display/input processes before =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|plumos-library-scan" | grep -v grep || true

if [ "$PLUMOS_FRONTEND_MALI_SCAN" = "1" ] && [ -x "$SCAN" ]; then
  "$SCAN" --defer-thumbnails >"$SCAN_LOG" 2>&1 || {
    echo "== scan log =="
    cat "$SCAN_LOG" || true
    exit 1
  }
  echo "== scan summary =="
  grep -E "^(mode:|summary|timing|wrote:)" "$SCAN_LOG" || true
fi

set +e
if [ -n "$PLUMOS_FRONTEND_MALI_SCRIPT" ]; then
  "$UI" --timeout "$PLUMOS_FRONTEND_MALI_TIMEOUT" \
    --script "$PLUMOS_FRONTEND_MALI_SCRIPT"
else
  "$UI" --timeout "$PLUMOS_FRONTEND_MALI_TIMEOUT"
fi
rc=$?
set -e
echo "== display/input processes after =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|plumos-library-scan" | grep -v grep || true
echo "result=frontend_mali_renderer_rc_${rc}"
exit "$rc"
'

A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/run-a30.sh" \
  "PLUMOS_FRONTEND_MALI_TIMEOUT=$(shell_quote "$TIMEOUT")
PLUMOS_FRONTEND_MALI_SCRIPT=$(shell_quote "$SCRIPT")
PLUMOS_FRONTEND_MALI_EXERCISE=$(shell_quote "$EXERCISE")
PLUMOS_FRONTEND_MALI_SCAN=$(shell_quote "$RUN_SCAN")
$remote_script"
