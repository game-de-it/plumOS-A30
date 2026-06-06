#!/bin/sh
set -eu

BASE_DIR="${PLUMOS_SSH_HOME:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
BIN_DIR="${BASE_DIR}/bin"
ETC_DIR="${BASE_DIR}/etc"
RUN_DIR="${BASE_DIR}/run"
LOG_DIR="${BASE_DIR}/log"
PORT="${PLUMOS_SSH_PORT:-2222}"
LISTEN="${PLUMOS_SSH_LISTEN:-0.0.0.0:${PORT}}"
AUTH_SOURCE="${PLUMOS_SSH_AUTHORIZED_KEYS:-${ETC_DIR}/authorized_keys}"
HOST_KEY="${ETC_DIR}/dropbear_ed25519_host_key"
PID_FILE="${RUN_DIR}/dropbear.pid"
LOG_FILE="${LOG_DIR}/dropbear.log"

mkdir -p "$ETC_DIR" "$RUN_DIR" "$LOG_DIR"

has_authorized_key() {
  [ -f "$AUTH_SOURCE" ] || return 1
  grep -Eq '^[[:space:]]*(ssh-rsa|ssh-ed25519|ecdsa-sha2-|sk-ssh-ed25519|sk-ecdsa-sha2-)' "$AUTH_SOURCE"
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

if ! has_authorized_key; then
  {
    echo "No SSH public key found in ${AUTH_SOURCE}."
    echo "Edit plumos/ssh/etc/authorized_keys on the SD card, then start SSH again."
  } | tee -a "$LOG_FILE"
  exit 1
fi

if [ ! -s "$HOST_KEY" ]; then
  echo "Generating Dropbear ed25519 host key at ${HOST_KEY}" | tee -a "$LOG_FILE"
  "$BIN_DIR/dropbearkey" -t ed25519 -f "$HOST_KEY" >> "$LOG_FILE" 2>&1
fi

if [ -s "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "Dropbear already running on ${LISTEN} with pid $(cat "$PID_FILE")" | tee -a "$LOG_FILE"
  write_network_snapshot
  exit 0
fi

rm -f "$PID_FILE"
write_network_snapshot

echo "Starting Dropbear on ${LISTEN}" | tee -a "$LOG_FILE"
"$BIN_DIR/dropbear" \
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
