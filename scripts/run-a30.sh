#!/usr/bin/env bash
set -euo pipefail

TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 COMMAND [ARGS...]

Defaults:
  A30_TARGET   ${TARGET}
  A30_SSH_PORT ${PORT}
EOF
}

if [ "$#" -eq 0 ] || [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

ssh "${SSH_OPTS[@]}" "$TARGET" "$@"
