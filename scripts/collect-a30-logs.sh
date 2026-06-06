#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${ROOT_DIR}/artifacts/a30-logs-${STAMP}"
ARCHIVE="${OUT_DIR}/plumos-logs.tar.gz"
SUMMARY="${OUT_DIR}/summary.txt"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

mkdir -p "$OUT_DIR"

echo "Collecting logs from ${TARGET}"
{
  echo "target: ${TARGET}"
  echo "port: ${PORT}"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  ssh "${SSH_OPTS[@]}" "$TARGET" "date; uname -a; ps | grep -E 'MainUI|plumos|dropbear' | grep -v grep || true"
} > "$SUMMARY" 2>&1 || true

ssh "${SSH_OPTS[@]}" "$TARGET" '
  paths=""
  for path in /mnt/SDCARD/plumos/logs /mnt/SDCARD/plumos/ssh/log /tmp/rc.local.log; do
    [ -e "$path" ] && paths="${paths} ${path}"
  done
  [ -n "$paths" ] || exit 2
  tar -czf - $paths 2>/dev/null
' > "$ARCHIVE" || {
    echo "warning: log archive collection failed; see ${SUMMARY}" >&2
  }

echo "Wrote ${OUT_DIR}"
