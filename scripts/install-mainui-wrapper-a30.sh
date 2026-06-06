#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

"${ROOT_DIR}/scripts/build-bootstrap-package.sh" >/dev/null
"${ROOT_DIR}/scripts/deploy-a30.sh" "${ROOT_DIR}/dist/plumos-bootstrap" /mnt/SDCARD

ssh "${SSH_OPTS[@]}" "$TARGET" 'sh -s' <<'REMOTE'
set -eu

APP_DIR=/mnt/SDCARD/miyoo/app
CURRENT="${APP_DIR}/MainUI"
BACKUP="${APP_DIR}/MainUI.stock"
WRAPPER=/mnt/SDCARD/plumos/bootstrap/MainUI.wrapper
LOG_DIR=/mnt/SDCARD/plumos/logs
STAMP="$(date +%Y%m%d-%H%M%S 2>/dev/null || echo unknown-time)"

mkdir -p "$LOG_DIR"

if [ ! -x "$WRAPPER" ]; then
  echo "error: wrapper is missing or not executable: $WRAPPER" >&2
  exit 1
fi

if [ ! -e "$CURRENT" ]; then
  echo "error: current MainUI is missing: $CURRENT" >&2
  exit 1
fi

if [ ! -e "$BACKUP" ]; then
  if grep -q "plumOS MainUI wrapper" "$CURRENT" 2>/dev/null; then
    echo "error: current MainUI already looks like wrapper, but backup is missing: $BACKUP" >&2
    exit 1
  fi
  cp "$CURRENT" "$BACKUP"
  chmod 0755 "$BACKUP"
  echo "created stock backup: $BACKUP"
else
  echo "preserving existing stock backup: $BACKUP"
fi

if [ -e "${CURRENT}.pre-plumos-${STAMP}" ]; then
  rm -f "${CURRENT}.pre-plumos-${STAMP}"
fi

if ! grep -q "plumOS MainUI wrapper" "$CURRENT" 2>/dev/null; then
  cp "$CURRENT" "${CURRENT}.pre-plumos-${STAMP}" 2>/dev/null || true
fi

cp "$WRAPPER" "$CURRENT"
chmod 0755 "$CURRENT"

cat > /mnt/SDCARD/plumos/bootstrap/mainui-wrapper-installed.txt <<EOF
installed_at=${STAMP}
current=${CURRENT}
backup=${BACKUP}
wrapper=${WRAPPER}
EOF

echo "installed plumOS MainUI wrapper"
echo "backup: $BACKUP"
echo "disable: touch /mnt/SDCARD/plumos/config/disable-mainui-wrapper"
REMOTE
