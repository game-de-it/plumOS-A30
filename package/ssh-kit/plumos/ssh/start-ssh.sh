#!/bin/sh
set -eu

BASE_DIR="${PLUMOS_SSH_HOME:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
BIN_DIR="${BASE_DIR}/bin"
ETC_DIR="${BASE_DIR}/etc"
RUN_DIR="${BASE_DIR}/run"
LOG_DIR="${BASE_DIR}/log"
PORT="${PLUMOS_SSH_PORT:-2222}"
LISTEN="${PLUMOS_SSH_LISTEN:-0.0.0.0:${PORT}}"
AUTH_SOURCE="${PLUMOS_SSH_AUTHORIZED_KEYS:-${ETC_DIR}/authorized_keys}"
PASSWORD_HASH="${PLUMOS_SSH_PASSWORD_HASH:-${ETC_DIR}/password.hash}"
HOST_KEY="${ETC_DIR}/dropbear_ed25519_host_key"
PID_FILE="${RUN_DIR}/dropbear.pid"
LOG_FILE="${LOG_DIR}/dropbear.log"
DROPBEAR_BIN="${PLUMOS_SSH_DROPBEAR_BIN:-${BIN_DIR}/dropbear}"
DEFAULT_PASSWORD_HASH='root:$6$plumOSa30$/RoWGjQmmhR5hxn5re2ORRFqzDnDyI4uVI7zEO0n3ts.qtqX7PX3MSw9/BOxOj0gxsJBcMA7zDHDWzZb674Jj0'
PLUMOS_SSH_DEFAULT_PATH="${PLUMOS_ROOT}/gnu/bin:${PLUMOS_ROOT}/bin:/usr/miyoo/bin:/usr/sbin:/usr/bin:/sbin:/bin"
PATH="${PLUMOS_SSH_PATH:-${PLUMOS_SSH_DEFAULT_PATH}}"
export PATH PLUMOS_ROOT

if [ -z "${PLUMOS_SSH_DROPBEAR_BIN:-}" ] && [ -x "${BIN_DIR}/dropbear-sftp" ]; then
  DROPBEAR_BIN="${BIN_DIR}/dropbear-sftp"
fi

mkdir -p "$ETC_DIR" "$RUN_DIR" "$LOG_DIR"

has_authorized_key() {
  [ -f "$AUTH_SOURCE" ] || return 1
  grep -Eq '^[[:space:]]*(ssh-rsa|ssh-ed25519|ecdsa-sha2-|sk-ssh-ed25519|sk-ecdsa-sha2-)' "$AUTH_SOURCE"
}

has_password_hash() {
  [ -f "$PASSWORD_HASH" ] || return 1
  grep -Eq '^[[:space:]]*root:[^[:space:]]+' "$PASSWORD_HASH"
}

find_existing_pid() {
  ps w 2>/dev/null | awk -v bin="$DROPBEAR_BIN" -v listen="$LISTEN" '
    index($0, bin) && index($0, "-p " listen) && $0 !~ / -2( |$)/ {
      print $1
      exit
    }
  '
}

ensure_password_hash() {
  if has_password_hash; then
    return 0
  fi
  {
    echo '# Default SSH password is "plumos".'
    echo '# Format is username:crypt(3)-hash. Replace the hash to change the password.'
    printf '%s\n' "$DEFAULT_PASSWORD_HASH"
  } > "$PASSWORD_HASH"
  chmod 0600 "$PASSWORD_HASH" 2>/dev/null || true
}

write_network_snapshot() {
  {
    echo "date: $(date 2>/dev/null || true)"
    echo
    for cmd in "ip addr" "ifconfig -a" "route -n" "netstat -an"; do
      set -- $cmd
      echo "## $cmd"
      if command -v "$1" >/dev/null 2>&1; then
        "$@" 2>&1 || true
      else
        echo "$1: not found"
      fi
      echo
    done
    for file in /proc/net/dev /proc/net/wireless /proc/net/route /proc/net/fib_trie; do
      echo "## $file"
      cat "$file" 2>&1 || true
      echo
    done
  } > "${LOG_DIR}/network.txt" 2>&1
}

ensure_password_hash
if has_authorized_key; then
  echo "SSH public key auth enabled from ${AUTH_SOURCE}" | tee -a "$LOG_FILE"
else
  echo "No SSH public key found in ${AUTH_SOURCE}; password auth remains enabled." | tee -a "$LOG_FILE"
fi
echo "SSH password hash file: ${PASSWORD_HASH}" | tee -a "$LOG_FILE"

if [ ! -s "$HOST_KEY" ]; then
  echo "Generating Dropbear ed25519 host key at ${HOST_KEY}" | tee -a "$LOG_FILE"
  "$BIN_DIR/dropbearkey" -t ed25519 -f "$HOST_KEY" >> "$LOG_FILE" 2>&1
fi

if [ -s "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "Dropbear already running on ${LISTEN} with pid $(cat "$PID_FILE")" | tee -a "$LOG_FILE"
  write_network_snapshot
  exit 0
fi

existing_pid="$(find_existing_pid || true)"
if [ -n "$existing_pid" ] && kill -0 "$existing_pid" 2>/dev/null; then
  printf '%s\n' "$existing_pid" > "$PID_FILE"
  echo "Dropbear already listening on ${LISTEN} with pid ${existing_pid}" | tee -a "$LOG_FILE"
  write_network_snapshot
  exit 0
fi

rm -f "$PID_FILE"
write_network_snapshot

echo "Starting Dropbear on ${LISTEN}" | tee -a "$LOG_FILE"
"$DROPBEAR_BIN" \
  -E \
  -m \
  -p "$LISTEN" \
  -P "$PID_FILE" \
  -r "$HOST_KEY" \
  -D "$ETC_DIR" \
  -T 3 \
  >> "$LOG_FILE" 2>&1

status=$?
if [ "$status" -eq 0 ]; then
  echo "Dropbear started. Connect with: ssh -p ${PORT} root@A30_IP_ADDRESS" | tee -a "$LOG_FILE"
else
  echo "Dropbear failed with status ${status}; see ${LOG_FILE}" | tee -a "$LOG_FILE"
fi

exit "$status"
