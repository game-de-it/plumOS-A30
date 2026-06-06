#!/usr/bin/env bash
set -euo pipefail

TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

ssh "${SSH_OPTS[@]}" "$TARGET" 'sh -s' <<'REMOTE'
set -eu

APP_DIR=/mnt/SDCARD/miyoo/app
CURRENT="${APP_DIR}/MainUI"
BACKUP="${APP_DIR}/MainUI.stock"

if [ ! -e "$BACKUP" ]; then
  echo "error: stock backup is missing: $BACKUP" >&2
  exit 1
fi

cp "$BACKUP" "$CURRENT"
chmod 0755 "$CURRENT"
rm -f /mnt/SDCARD/plumos/config/disable-mainui-wrapper

echo "restored stock MainUI from $BACKUP"
REMOTE
