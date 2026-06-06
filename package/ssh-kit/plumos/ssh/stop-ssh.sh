#!/bin/sh
set -eu

BASE_DIR="${PLUMOS_SSH_HOME:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
PID_FILE="${BASE_DIR}/run/dropbear.pid"
LOG_FILE="${BASE_DIR}/log/dropbear.log"

mkdir -p "$(dirname "$LOG_FILE")"

if [ ! -s "$PID_FILE" ]; then
  echo "No Dropbear pid file found" | tee -a "$LOG_FILE"
  exit 0
fi

pid="$(cat "$PID_FILE")"
if kill -0 "$pid" 2>/dev/null; then
  kill "$pid"
  echo "Stopped Dropbear pid ${pid}" | tee -a "$LOG_FILE"
else
  echo "Dropbear pid ${pid} is not running" | tee -a "$LOG_FILE"
fi

rm -f "$PID_FILE"

