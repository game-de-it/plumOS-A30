#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
HOST="${1:-}"
PORT="${A30_SSH_PORT:-2222}"

if [ -z "$HOST" ]; then
  printf 'usage: %s root@A30_IP_ADDRESS [output-dir]\n' "$0" >&2
  exit 2
fi

OUT_DIR="${2:-${ROOT_DIR}/artifacts/a30-info-$(date +%Y%m%d-%H%M%S)}"
KNOWN_HOSTS="${ROOT_DIR}/artifacts/known_hosts"
mkdir -p "$OUT_DIR" "$(dirname "$KNOWN_HOSTS")"

ssh_args=(
  -p "$PORT"
  -o StrictHostKeyChecking=accept-new
  -o UserKnownHostsFile="$KNOWN_HOSTS"
)

if [ -n "${A30_SSH_KEY:-}" ]; then
  ssh_args+=(-i "$A30_SSH_KEY")
fi

printf 'Collecting A30 information from %s on port %s...\n' "$HOST" "$PORT"

ssh "${ssh_args[@]}" "$HOST" 'sh -s' > "${OUT_DIR}/system.txt" <<'REMOTE'
section() {
  printf '\n## %s\n' "$*"
}

run() {
  section "$*"
  "$@" 2>&1 || true
}

run date
run id
run uname -a
run uptime
run cat /proc/cmdline
run cat /proc/cpuinfo
run cat /proc/meminfo
run cat /proc/version
run cat /proc/filesystems
run cat /proc/modules
run cat /proc/partitions
run cat /proc/mounts
run mount
run df -h
run ps w
run cat /etc/passwd

section "network commands"
for cmd in "ip addr" "ifconfig -a" "route -n" "netstat -an"; do
  set -- $cmd
  if command -v "$1" >/dev/null 2>&1; then
    "$@" 2>&1 || true
  else
    printf '%s: not found\n' "$1"
  fi
done

run cat /proc/net/dev
run cat /proc/net/wireless
run cat /proc/net/route
run cat /proc/net/fib_trie

section "important directories"
for path in / /bin /sbin /etc /lib /usr /mnt /mnt/SDCARD /tmp /var /root; do
  printf '\n# %s\n' "$path"
  ls -la "$path" 2>&1 || true
done

section "sd scripts and configs"
find /mnt/SDCARD -maxdepth 4 -type f \( -name '*.sh' -o -name '*.conf' -o -name '*.ini' -o -name '*.json' \) 2>/dev/null | sort | head -300

section "environment"
env 2>&1 || true
REMOTE

if ssh "${ssh_args[@]}" "$HOST" 'tar -C /mnt/SDCARD/plumos/ssh -czf - log 2>/dev/null' > "${OUT_DIR}/ssh-log.tar.gz"; then
  printf 'Saved SSH logs to %s\n' "${OUT_DIR}/ssh-log.tar.gz"
else
  rm -f "${OUT_DIR}/ssh-log.tar.gz"
fi

printf 'Saved system report to %s\n' "${OUT_DIR}/system.txt"

