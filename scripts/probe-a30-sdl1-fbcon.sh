#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/sdl1-fbcon}"
MODE="${MODE:-640x480}"
BPP="${BPP:-16}"
RUN_MS="${RUN_MS:-15000}"
FPS="${FPS:-30}"
UPDATE="${UPDATE:-rect}"
FULLSCREEN="${FULLSCREEN:-1}"
DOUBLEBUF="${DOUBLEBUF:-0}"
HWSURFACE="${HWSURFACE:-0}"
DEPLOY="${DEPLOY:-0}"
RESTART_FE="${RESTART_FE:-1}"

usage() {
  cat <<EOF
Usage: $0 [--deploy] [--mode WxH] [--bpp N] [--run-ms N] [--fps N]
          [--update rect|flip] [--fullscreen 0|1] [--doublebuf 0|1]
          [--hwsurface 0|1] [--no-restart-fe]

Runs the stock SDL1 fbcon draw probe on the A30, captures /dev/fb0, and restores
the plumOS FE unless --no-restart-fe is used.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --mode)
      MODE="${2:?missing mode}"
      shift 2
      ;;
    --bpp)
      BPP="${2:?missing bpp}"
      shift 2
      ;;
    --run-ms)
      RUN_MS="${2:?missing run-ms}"
      shift 2
      ;;
    --fps)
      FPS="${2:?missing fps}"
      shift 2
      ;;
    --update)
      UPDATE="${2:?missing update}"
      shift 2
      ;;
    --fullscreen)
      FULLSCREEN="${2:?missing fullscreen}"
      shift 2
      ;;
    --doublebuf)
      DOUBLEBUF="${2:?missing doublebuf}"
      shift 2
      ;;
    --hwsurface)
      HWSURFACE="${2:?missing hwsurface}"
      shift 2
      ;;
    --no-restart-fe)
      RESTART_FE=0
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

STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${STAMP}-${MODE}-${UPDATE}"
REMOTE_LOG="/mnt/SDCARD/plumos/logs/sdl1-fbcon-probe-last.log"
REMOTE_PID="/tmp/plumos-sdl1-fbcon-probe.pid"
mkdir -p "${OUT_DIR}"

cleanup() {
  if [ "${RESTART_FE}" = 1 ]; then
    "${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >/dev/null 2>&1 || true
    "${ROOT_DIR}/scripts/a30-fe-control.sh" start >/dev/null 2>&1 || true
    "${ROOT_DIR}/scripts/a30-fe-control.sh" status >"${OUT_DIR}/fe-status-after.log" 2>&1 || true
  fi
}
trap cleanup EXIT

if [ "${DEPLOY}" = 1 ]; then
  "${ROOT_DIR}/scripts/deploy-a30.sh" "${ROOT_DIR}/dist/plumos-sdl1-fbcon-probe" /mnt/SDCARD
fi

"${ROOT_DIR}/scripts/a30-fe-control.sh" stop >"${OUT_DIR}/fe-stop.log" 2>&1 || true
"${ROOT_DIR}/scripts/a30-fe-control.sh" status >"${OUT_DIR}/fe-status-before.log" 2>&1 || true

"${ROOT_DIR}/scripts/run-a30.sh" "sh -c 'trap \"\" HUP; mkdir -p /mnt/SDCARD/plumos/logs; SDL_VIDEODRIVER=fbcon SDL_FBDEV=/dev/fb0 SDL_NOMOUSE=1 /mnt/SDCARD/plumos/bin/plumos-sdl1-fbcon-probe --mode ${MODE} --bpp ${BPP} --run-ms ${RUN_MS} --fps ${FPS} --update ${UPDATE} --fullscreen ${FULLSCREEN} --doublebuf ${DOUBLEBUF} --hwsurface ${HWSURFACE} >${REMOTE_LOG} 2>&1 & echo \$! >${REMOTE_PID}; echo probe_pid=\$(cat ${REMOTE_PID})'" \
  >"${OUT_DIR}/launch.log"

sleep 2
"${ROOT_DIR}/scripts/a30-fe-control.sh" status >"${OUT_DIR}/status-during.log" 2>&1 || true
"${ROOT_DIR}/scripts/capture-a30-framebuffer.sh" "${OUT_DIR}" >"${OUT_DIR}/capture.log" 2>&1 || true

WAIT_SEC=$((RUN_MS / 1000 + 5))
"${ROOT_DIR}/scripts/run-a30.sh" "sh -c 'pid=\$(cat ${REMOTE_PID} 2>/dev/null || true); if [ -n \"\$pid\" ]; then i=0; while kill -0 \"\$pid\" 2>/dev/null && [ \$i -lt ${WAIT_SEC} ]; do sleep 1; i=\$((i+1)); done; kill -TERM \"\$pid\" 2>/dev/null || true; fi; cat ${REMOTE_LOG} 2>/dev/null || true'" \
  >"${OUT_DIR}/remote.log" 2>&1 || true

printf 'out_dir=%s\n' "${OUT_DIR}"
printf 'remote_log=%s\n' "${OUT_DIR}/remote.log"
find "${OUT_DIR}" -maxdepth 1 -type f | sort
