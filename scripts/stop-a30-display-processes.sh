#!/usr/bin/env bash
set -euo pipefail

TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)
DRY_RUN=0

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--dry-run]

Stops A30 frontend/emulator/display test processes robustly, including binaries
launched through ld-linux-armhf whose process name is not the frontend/emulator
name.

Defaults:
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --dry-run|-n)
      DRY_RUN=1
      ;;
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
  shift
done

ssh "${SSH_OPTS[@]}" "$TARGET" "DRY_RUN=${DRY_RUN} sh -s" <<'REMOTE'
set -u

add_pid() {
  pid=$1
  case "$pid" in
    ""|*[!0-9]*) return 0 ;;
  esac
  [ "$pid" = "$$" ] && return 0
  case " $pids " in
    *" $pid "*) return 0 ;;
  esac
  pids="$pids $pid"
}

pids=""

cmd_is_display_target() {
  cmd=$1
  case "$cmd" in
    *'/mnt/SDCARD/plumos/emulators/'*|\
    *'/mnt/SDCARD/plumos/bin/plumos-controller-ui'*|\
    *'/mnt/SDCARD/plumos/bin/plumos-pyxel-a30'*|\
    *'/mnt/SDCARD/plumos/python/bin/python3.14 -m pyxel '*|\
    *'/mnt/SDCARD/plumos/retroarch/bin/retroarch'*|\
    *'/mnt/SDCARD/plumos/bin/plumos-standalone-launch'*|\
    *PPSSPPSDL*|*pcsx_rearmed*|*scummvm*|*dosbox*|*easyrpg-player*|*red-viper-a30*)
      return 0
      ;;
  esac
  return 1
}

for fd in /proc/[0-9]*/fd/*; do
  target=$(readlink "$fd" 2>/dev/null || true)
  if [ "$target" = /dev/fb0 ]; then
    pid=${fd#/proc/}
    pid=${pid%%/*}
    cmd=$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)
    if cmd_is_display_target "$cmd"; then
      add_pid "$pid"
    else
      printf 'skip_fb0_pid=%s COMM=' "$pid"
      cat "/proc/$pid/comm" 2>/dev/null || true
      printf 'CMD='
      printf '%s\n' "$cmd"
    fi
  fi
done

for f in /proc/[0-9]*/cmdline; do
  pid=${f#/proc/}
  pid=${pid%/cmdline}
  [ "$pid" = "$$" ] && continue
  cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
  cmd_is_display_target "$cmd" && add_pid "$pid"
done

echo "target_pids=${pids:- none}"
for pid in $pids; do
  printf 'PID=%s COMM=' "$pid"
  cat "/proc/$pid/comm" 2>/dev/null || true
  printf 'CMD='
  tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true
  echo
done

if [ "${DRY_RUN:-0}" = 1 ]; then
  exit 0
fi

if [ -n "$pids" ]; then
  kill -TERM $pids 2>/dev/null || true
  sleep 1
  kill -KILL $pids 2>/dev/null || true
fi

if [ -x /mnt/SDCARD/plumos/bin/plumos-fb-restore ]; then
  /mnt/SDCARD/plumos/bin/plumos-fb-restore --quiet >/dev/null 2>&1 || true
elif command -v fbset >/dev/null 2>&1; then
  fbset -g 480 640 480 1280 32 >/dev/null 2>&1 || true
fi

remaining=0
for fd in /proc/[0-9]*/fd/*; do
  target=$(readlink "$fd" 2>/dev/null || true)
  if [ "$target" = /dev/fb0 ]; then
    remaining=1
    pid=${fd#/proc/}
    pid=${pid%%/*}
    printf 'remaining_fb0_pid=%s COMM=' "$pid"
    cat "/proc/$pid/comm" 2>/dev/null || true
    printf 'CMD='
    tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true
    echo
  fi
done
[ "$remaining" = 0 ] && echo "remaining_fb0_pid=none"
REMOTE
