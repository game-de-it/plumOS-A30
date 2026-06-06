#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SRC="${1:-${ROOT_DIR}/dist/plumos-runtime}"
REMOTE_DIR="${2:-${A30_REMOTE_DIR:-/mnt/SDCARD/plumos}}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [LOCAL_PATH] [REMOTE_DIR]

Defaults:
  LOCAL_PATH  ${ROOT_DIR}/dist/plumos-runtime
  REMOTE_DIR  /mnt/SDCARD/plumos
  A30_TARGET  ${TARGET}
  A30_SSH_PORT ${PORT}
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

if [ ! -e "$SRC" ]; then
  echo "error: local path does not exist: $SRC" >&2
  exit 1
fi

if [ -f "$SRC" ]; then
  SRC_PARENT="$(dirname "$SRC")"
  SRC_BASE="$(basename "$SRC")"
  TAR_ARGS=(-C "$SRC_PARENT" "$SRC_BASE")
else
  TAR_ARGS=(-C "$SRC" .)
fi

echo "Deploying ${SRC} to ${TARGET}:${REMOTE_DIR}"
ssh "${SSH_OPTS[@]}" "$TARGET" "mkdir -p '$REMOTE_DIR'"
tar -cf - "${TAR_ARGS[@]}" | ssh "${SSH_OPTS[@]}" "$TARGET" "tar -xf - -C '$REMOTE_DIR'"
echo "Deploy complete"
