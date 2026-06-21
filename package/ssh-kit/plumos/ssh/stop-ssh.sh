#!/bin/sh
set -eu

BASE_DIR="${PLUMOS_SSH_HOME:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
BIN_DIR="${BASE_DIR}/bin"
PORT="${PLUMOS_SSH_PORT:-2222}"
LISTEN="${PLUMOS_SSH_LISTEN:-0.0.0.0:${PORT}}"
DROPBEAR_BIN="${PLUMOS_SSH_DROPBEAR_BIN:-${BIN_DIR}/dropbear}"
PID_FILE="${BASE_DIR}/run/dropbear.pid"
LOG_FILE="${BASE_DIR}/log/dropbear.log"

if [ -z "${PLUMOS_SSH_DROPBEAR_BIN:-}" ] && [ -x "${BIN_DIR}/dropbear-sftp" ]; then
  DROPBEAR_BIN="${BIN_DIR}/dropbear-sftp"
fi

mkdir -p "$(dirname "$LOG_FILE")"

find_existing_pid() {
  ps w 2>/dev/null | awk -v bin="$DROPBEAR_BIN" -v listen="$LISTEN" '
    index($0, bin) && index($0, "-p " listen) && $0 !~ / -2( |$)/ {
      print $1
      exit
    }
  '
}

if [ -s "$PID_FILE" ]; then
  pid="$(cat "$PID_FILE")"
else
  pid="$(find_existing_pid || true)"
fi

if [ -z "$pid" ]; then
  echo "No Dropbear pid found for ${LISTEN}" | tee -a "$LOG_FILE"
  rm -f "$PID_FILE"
  exit 0
fi

if kill -0 "$pid" 2>/dev/null; then
  kill "$pid"
  echo "Stopped Dropbear pid ${pid}" | tee -a "$LOG_FILE"
else
  echo "Dropbear pid ${pid} is not running" | tee -a "$LOG_FILE"
fi

rm -f "$PID_FILE"
