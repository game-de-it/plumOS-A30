#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SRC="${1:-${ROOT_DIR}/dist/plumos-runtime}"
REMOTE_DIR="${2:-${A30_REMOTE_DIR:-/mnt/SDCARD/plumos}}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)
PROTECT_CONFIG="${PLUMOS_DEPLOY_PROTECT_CONFIG:-1}"
PRESERVE_DIR="/tmp/plumos-deploy-preserve-$$"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [LOCAL_PATH] [REMOTE_DIR]

Defaults:
  LOCAL_PATH  ${ROOT_DIR}/dist/plumos-runtime
  REMOTE_DIR  /mnt/SDCARD/plumos
  A30_TARGET  ${TARGET}
  A30_SSH_PORT ${PORT}
  PLUMOS_DEPLOY_PROTECT_CONFIG
              Preserve existing plumOS mutable settings. Default: 1.
EOF
}

shell_quote() {
  printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
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

remote_env_prefix() {
  printf 'REMOTE_DIR=%s PRESERVE_DIR=%s' "$(shell_quote "$REMOTE_DIR")" "$(shell_quote "$PRESERVE_DIR")"
}

preserve_mutable_config() {
  [ "$PROTECT_CONFIG" = "1" ] || return 0
  local remote_env
  remote_env="$(remote_env_prefix)"
  ssh "${SSH_OPTS[@]}" "$TARGET" "$remote_env sh -s" <<'REMOTE'
set -eu
mkdir -p "$REMOTE_DIR" "$PRESERVE_DIR"
: > "${PRESERVE_DIR}/manifest"
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  src="${REMOTE_DIR}/${rel}"
  dst="${PRESERVE_DIR}/${rel}"
  if [ -e "$src" ]; then
    mkdir -p "$(dirname "$dst")"
    cp -p "$src" "$dst" 2>/dev/null || cp "$src" "$dst"
    printf '%s\n' "$rel" >> "${PRESERVE_DIR}/manifest"
  fi
done <<'PATHS'
plumos/config/frontend/settings.json
plumos/config/system/settings.json
plumos/config/network/settings.json
plumos/config/performance/settings.json
config/frontend/settings.json
config/system/settings.json
config/network/settings.json
config/performance/settings.json
PATHS
REMOTE
}

restore_mutable_config() {
  [ "$PROTECT_CONFIG" = "1" ] || return 0
  local remote_env
  remote_env="$(remote_env_prefix)"
  ssh "${SSH_OPTS[@]}" "$TARGET" "$remote_env sh -s" <<'REMOTE'
set -eu
if [ -r "${PRESERVE_DIR}/manifest" ]; then
  while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    src="${PRESERVE_DIR}/${rel}"
    dst="${REMOTE_DIR}/${rel}"
    [ -e "$src" ] || continue
    mkdir -p "$(dirname "$dst")"
    cp -p "$src" "$dst" 2>/dev/null || cp "$src" "$dst"
    echo "preserved mutable config: ${dst}"
  done < "${PRESERVE_DIR}/manifest"
fi
rm -rf "$PRESERVE_DIR"
REMOTE
}

echo "Deploying ${SRC} to ${TARGET}:${REMOTE_DIR}"
ssh "${SSH_OPTS[@]}" "$TARGET" "mkdir -p $(shell_quote "$REMOTE_DIR")"
preserve_mutable_config
set +e
COPYFILE_DISABLE=1 tar -cf - "${TAR_ARGS[@]}" |
  ssh "${SSH_OPTS[@]}" "$TARGET" "tar -xf - -C $(shell_quote "$REMOTE_DIR")"
deploy_status="$?"
set -e
restore_mutable_config
if [ "$deploy_status" -ne 0 ]; then
  echo "error: deploy failed" >&2
  exit "$deploy_status"
fi
echo "Deploy complete"
