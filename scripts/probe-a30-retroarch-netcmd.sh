#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SYSTEM="${1:-nes}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/retroarch-netcmd}"
ALLOW_STATE_OVERWRITE=0
RESTART_FE=1
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [SYSTEM] [options]

Launches plumOS RetroArch directly, exercises Network Control Interface
SAVE_STATE, SAVE_FILES, CLOSE_CONTENT, and QUIT, then verifies cleanup.

SYSTEM:
  nes
  gb

Options:
  --allow-state-overwrite
                      Allow overwriting an existing target ROM .state file.
  --no-restart-fe     Leave the FE stopped after the probe.
  -h, --help          Show this help.

Defaults:
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
  OUT_ROOT      ${OUT_ROOT}
EOF
}

if [ "${SYSTEM}" = "-h" ] || [ "${SYSTEM}" = "--help" ]; then
  usage
  exit 0
fi
if [ "$#" -gt 0 ]; then
  shift
fi

while [ "$#" -gt 0 ]; do
  case "$1" in
    --allow-state-overwrite)
      ALLOW_STATE_OVERWRITE=1
      shift
      ;;
    --no-restart-fe)
      RESTART_FE=0
      shift
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
done

case "$SYSTEM" in
  nes|gb) ;;
  *) echo "error: unsupported SYSTEM: $SYSTEM" >&2; exit 2 ;;
esac

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}-${SYSTEM}-save-close"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/pre-stop.txt" 2>&1 || true

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "SYSTEM='$SYSTEM' ALLOW_STATE_OVERWRITE='$ALLOW_STATE_OVERWRITE' RESTART_FE='$RESTART_FE' sh -s" <<'REMOTE' \
  >"${OUT_DIR}/remote.log" 2>&1
set -u

PLUMOS_ROOT=/mnt/SDCARD/plumos
SYSTEM=${SYSTEM:?}
ALLOW_STATE_OVERWRITE=${ALLOW_STATE_OVERWRITE:-0}
RESTART_FE=${RESTART_FE:-1}
UDP="$PLUMOS_ROOT/bin/plumos-udp-send"
LAUNCHER="$PLUMOS_ROOT/bin/plumos-retroarch-launch"
LAUNCH_LOG=/tmp/plumos-ra-netcmd-save-close-launcher.log
STATUS_LOG=/tmp/plumos-ra-netcmd-save-close-status.log
FRONTEND_LOG=/tmp/plumos-frontend-resume.log

cpu_line() {
  p=/sys/devices/system/cpu/cpu0/cpufreq
  printf 'gov=%s min=%s max=%s cur=%s online=%s\n' \
    "$(cat "$p/scaling_governor" 2>/dev/null || true)" \
    "$(cat "$p/scaling_min_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_max_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_cur_freq" 2>/dev/null || true)" \
    "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
}

interesting_ps() {
  ps w | grep -E 'plumos-controller-ui-mali|plumos-retroarch-launch|retroarch|plumos-joystickd' | grep -v grep || true
}

first_rom() {
  dir=$1
  pattern=$2
  find "$dir" -maxdepth 2 -type f -name "$pattern" 2>/dev/null |
    grep -v "/\\._" |
    head -n 1
}

kill_pid_quick() {
  pid=$1
  case "$pid" in
    ""|*[!0-9]*) return 0 ;;
  esac
  [ "$pid" = "$$" ] && return 0
  kill -TERM "$pid" 2>/dev/null || true
  sleep 1
  kill -KILL "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

restart_fe() {
  [ "$RESTART_FE" = 1 ] || return 0
  if [ -x "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" ] &&
     [ -z "$(ps w | grep -F "$PLUMOS_ROOT/bin/plumos-controller-ui-mali.bin" | grep -v grep || true)" ]; then
    trap '' HUP
    PLUMOS_FRONTEND_MODE=manual "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" \
      --rescue-network --rotation auto >"$FRONTEND_LOG" 2>&1 </dev/null &
    echo "frontend_restarted=1"
    echo "$!" >/tmp/plumos-controller-ui-mali.pid
  fi
}

send_cmd() {
  label=$1
  shift
  echo "== send $label =="
  "$UDP" "$@"
}

get_status() {
  label=$1
  echo "== status $label =="
  if "$UDP" --recv-ms 1500 GET_STATUS >"$STATUS_LOG" 2>&1; then
    cat "$STATUS_LOG"
    return 0
  fi
  cat "$STATUS_LOG"
  return 1
}

wait_for_status() {
  i=0
  while [ "$i" -lt 30 ]; do
    if get_status "wait-$i"; then
      return 0
    fi
    i=$((i + 1))
    sleep 0.25
  done
  return 1
}

find_state_file() {
  state_found=""
  for f in "$STATE_CORE_DIR/$ROM_STEM.state"*; do
    [ -f "$f" ] || continue
    if [ -s "$f" ]; then
      state_found=$f
      break
    fi
  done
  [ -n "$state_found" ] && echo "$state_found"
}

wait_for_state_file() {
  i=0
  while [ "$i" -lt 40 ]; do
    state_found="$(find_state_file || true)"
    if [ -n "$state_found" ]; then
      echo "$state_found"
      return 0
    fi
    i=$((i + 1))
    sleep 0.25
  done
  return 1
}

case "$SYSTEM" in
  nes)
    ROM="$(first_rom /mnt/SDCARD/Roms/FC "*.nes")"
    STATE_CORE_DIR="$PLUMOS_ROOT/retroarch/states/FCEUmm"
    ;;
  gb)
    ROM="$(first_rom /mnt/SDCARD/Roms/GB "*.gb")"
    STATE_CORE_DIR="$PLUMOS_ROOT/retroarch/states/Gambatte"
    ;;
esac

[ -x "$UDP" ] || { echo "error: missing $UDP"; exit 1; }
[ -x "$LAUNCHER" ] || { echo "error: missing $LAUNCHER"; exit 1; }
[ -n "$ROM" ] || { echo "error: no ROM found for system=$SYSTEM"; exit 1; }

ROM_BASE=${ROM##*/}
ROM_STEM=${ROM_BASE%.*}
mkdir -p "$STATE_CORE_DIR"

echo "system=$SYSTEM"
echo "rom=$ROM"
echo "state_core_dir=$STATE_CORE_DIR"
echo "rom_stem=$ROM_STEM"
echo "allow_state_overwrite=$ALLOW_STATE_OVERWRITE"
echo "== before =="
cpu_line
interesting_ps
echo "== existing target states =="
existing_state_count=0
for f in "$STATE_CORE_DIR/$ROM_STEM.state"*; do
  [ -f "$f" ] || continue
  existing_state_count=$((existing_state_count + 1))
  ls -l "$f"
done
echo "existing_state_count=$existing_state_count"
if [ "$existing_state_count" -gt 0 ] && [ "$ALLOW_STATE_OVERWRITE" != 1 ]; then
  echo "result=retroarch_netcmd_refuse_state_overwrite"
  exit 1
fi

rm -f "$LAUNCH_LOG" "$STATUS_LOG"
trap 'kill_pid_quick "${LAUNCH_PID:-}"; restart_fe' EXIT INT TERM

PLUMOS_RA_QUIT_PRESS_TWICE=false "$LAUNCHER" --system "$SYSTEM" --no-joystickd \
  --cpu fixed --freq 648000 --cores 2 >"$LAUNCH_LOG" 2>&1 &
LAUNCH_PID=$!
echo "launch_pid=$LAUNCH_PID"

if wait_for_status; then
  netcmd_ready=1
else
  netcmd_ready=0
fi
echo "netcmd_ready=$netcmd_ready"

echo "== during =="
cpu_line
interesting_ps
if [ -x "$PLUMOS_ROOT/gnu/bin/ss" ]; then
  "$PLUMOS_ROOT/gnu/bin/ss" -lunp 2>/dev/null | grep 55355 || true
fi

state_saved=0
close_content_ok=0
launch_exited=0

if [ "$netcmd_ready" = 1 ]; then
  send_cmd SAVE_STATE SAVE_STATE || true
  if saved_state="$(wait_for_state_file 2>/dev/null)"; then
    state_saved=1
    echo "saved_state=$saved_state"
    ls -l "$saved_state"
  else
    echo "saved_state="
  fi

  send_cmd SAVE_FILES SAVE_FILES || true
  sleep 1

  send_cmd CLOSE_CONTENT CLOSE_CONTENT || true
  sleep 1
  send_cmd CLOSE_CONTENT-second --repeat 1 CLOSE_CONTENT || true
  sleep 2
  if get_status after-close; then
    if grep -q 'CONTENTLESS' "$STATUS_LOG"; then
      close_content_ok=1
    fi
  elif ! kill -0 "$LAUNCH_PID" 2>/dev/null; then
    close_content_ok=1
  fi

  send_cmd QUIT --repeat 2 --delay-ms 500 QUIT || true
fi

i=0
while [ "$i" -lt 40 ]; do
  if ! kill -0 "$LAUNCH_PID" 2>/dev/null; then
    launch_exited=1
    break
  fi
  i=$((i + 1))
  sleep 0.25
done
if [ "$launch_exited" != 1 ]; then
  kill_pid_quick "$LAUNCH_PID"
else
  wait "$LAUNCH_PID" 2>/dev/null || true
fi
LAUNCH_PID=""
sync
trap - EXIT INT TERM

echo "== after =="
cpu_line
interesting_ps
echo "== target states after =="
for f in "$STATE_CORE_DIR/$ROM_STEM.state"*; do
  [ -f "$f" ] || continue
  ls -l "$f"
done
echo "== launcher log tail =="
tail -n 120 "$LAUNCH_LOG" 2>/dev/null || true

remaining="$(interesting_ps | grep -Ev 'plumos-controller-ui-mali|ash -c' || true)"
cpu_state="$(cpu_line)"
case "$cpu_state" in
  gov=ondemand\ min=120000\ max=1200000\ *online=0-3*) cpu_restored=1 ;;
  *) cpu_restored=0 ;;
esac
echo "state_saved=$state_saved"
echo "close_content_ok=$close_content_ok"
echo "launch_exited=$launch_exited"
echo "cpu_restored=$cpu_restored"
if [ -z "$remaining" ] && [ "$netcmd_ready" = 1 ] && [ "$state_saved" = 1 ] &&
   [ "$close_content_ok" = 1 ] && [ "$launch_exited" = 1 ] && [ "$cpu_restored" = 1 ]; then
  echo "result=retroarch_netcmd_save_close_quit_ok"
else
  echo "result=retroarch_netcmd_save_close_quit_check_logs"
fi

restart_fe
REMOTE

printf 'out_dir=%s\n' "$OUT_DIR"
cat "${OUT_DIR}/remote.log"
