#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/safe-power}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Runs dry-run checks for plumos-safe-shutdown poweroff and sleep backends.
This probe does not actually power off or suspend the A30.

Options:
  -h, --help            Show this help.

Defaults:
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
  OUT_ROOT      ${OUT_ROOT}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}-dry-run"
mkdir -p "$OUT_DIR"

ssh "${SSH_OPTS[@]}" "$TARGET" 'sh -s' <<'REMOTE' >"${OUT_DIR}/remote.log" 2>&1
set -u

PLUMOS_ROOT=/mnt/SDCARD/plumos
SAFE="$PLUMOS_ROOT/bin/plumos-safe-shutdown"
LOG="$PLUMOS_ROOT/logs/safe-shutdown.log"

echo "test=plumos_safe_power_probe"
date '+remote_time=%Y-%m-%dT%H:%M:%S%z' 2>/dev/null || true
echo "safe=$SAFE"
echo "== capabilities =="
for cmd in poweroff halt busybox; do
  printf '%s=' "$cmd"
  command -v "$cmd" 2>/dev/null || true
done
printf 'sys_power_state='
cat /sys/power/state 2>/dev/null || true
printf 'sysrq='
cat /proc/sys/kernel/sysrq 2>/dev/null || true
printf 'rtc='
[ -w /sys/class/rtc/rtc0/wakealarm ] && echo writable || echo unavailable

rm -f "$LOG"

echo "== dry-run shutdown power auto =="
"$SAFE" --dry-run --shutdown --poweroff --power-backend auto --no-hold-resume --wait-sec 1
echo "== dry-run shutdown power busybox =="
"$SAFE" --dry-run --shutdown --poweroff --power-backend busybox --no-hold-resume --wait-sec 1
echo "== dry-run sleep pseudo =="
"$SAFE" --dry-run --sleep --sleep-backend pseudo --no-hold-resume --wait-sec 1
echo "== dry-run sleep standby wakealarm =="
"$SAFE" --dry-run --sleep --sleep-backend standby --wakeup-sec 5 --no-hold-resume --wait-sec 1

echo "== log =="
cat "$LOG" 2>/dev/null || true

if grep -q 'poweroff dry_run backend=poweroff command=poweroff' "$LOG" 2>/dev/null &&
   grep -q 'poweroff dry_run backend=busybox command=/bin/busybox poweroff' "$LOG" 2>/dev/null &&
   grep -q 'sleep pseudo result=ok' "$LOG" 2>/dev/null &&
   grep -q 'sleep wakealarm dry_run .* wakeup_sec=5' "$LOG" 2>/dev/null &&
   grep -q 'sleep dry_run backend=standby path=/sys/power/state' "$LOG" 2>/dev/null; then
  echo "result=plumos_safe_power_probe_ok"
else
  echo "result=plumos_safe_power_probe_fail"
  exit 1
fi
REMOTE

echo "Artifact: ${OUT_DIR}"
if grep -q "result=plumos_safe_power_probe_ok" "${OUT_DIR}/remote.log"; then
  echo "Result: ok"
else
  echo "Result: failed"
  tail -n 100 "${OUT_DIR}/remote.log" >&2 || true
  exit 1
fi
