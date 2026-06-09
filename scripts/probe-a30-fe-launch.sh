#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SYSTEM="${1:-nes}"
RUN_SEC="${PLUMOS_FE_LAUNCH_RUN_SEC:-8}"
SETTLE_SEC="${PLUMOS_FE_LAUNCH_SETTLE_SEC:-2}"
RA_MAX_FRAMES="${PLUMOS_RA_MAX_FRAMES:-}"
RA_QUIT_SEC="${PLUMOS_RA_QUIT_SEC:-}"
EXPECT_EXECUTE_OK=0
RESTART_FE=1
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/fe-launch}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 SYSTEM [options]

Runs the Mali controller UI through its --script path, opens SYSTEM, presses A
on the selected ROM, lets the emulator run briefly, stops the launched emulator,
and verifies CPU/process cleanup and FE return.

SYSTEM:
  nes
  psx
  psp
  dos
  easyrpg
  scummvm

Options:
  --run-sec SEC       Seconds to let the launched emulator run. Default: ${RUN_SEC}
  --settle-sec SEC    Seconds to wait after launch detection before sampling. Default: ${SETTLE_SEC}
  --rom-index N       1-based ROM index in that system list. Default: 1
  --retroarch-max-frames N
                      Set PLUMOS_RA_MAX_FRAMES for RetroArch launch probes.
  --retroarch-quit-sec SEC
                      Send RetroArch network command QUIT after SEC seconds.
  --expect-execute-ok Require frontend-launch.log to contain execute: ok.
  --no-restart-fe     Leave the FE stopped after the probe.
  -h, --help          Show this help.

Defaults:
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
  OUT_ROOT      ${OUT_ROOT}
EOF
}

ROM_INDEX=1
if [ "${SYSTEM}" = "-h" ] || [ "${SYSTEM}" = "--help" ]; then
  usage
  exit 0
fi
if [ "$#" -gt 0 ]; then
  shift
fi
while [ "$#" -gt 0 ]; do
  case "$1" in
    --run-sec)
      RUN_SEC="${2:?missing --run-sec value}"
      shift 2
      ;;
    --settle-sec)
      SETTLE_SEC="${2:?missing --settle-sec value}"
      shift 2
      ;;
    --rom-index)
      ROM_INDEX="${2:?missing --rom-index value}"
      shift 2
      ;;
    --retroarch-max-frames)
      RA_MAX_FRAMES="${2:?missing --retroarch-max-frames value}"
      shift 2
      ;;
    --retroarch-quit-sec)
      RA_QUIT_SEC="${2:?missing --retroarch-quit-sec value}"
      shift 2
      ;;
    --expect-execute-ok)
      EXPECT_EXECUTE_OK=1
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

case "$RUN_SEC" in
  ""|*[!0-9]*) echo "error: --run-sec must be an integer: $RUN_SEC" >&2; exit 2 ;;
esac
case "$SETTLE_SEC" in
  ""|*[!0-9]*) echo "error: --settle-sec must be an integer: $SETTLE_SEC" >&2; exit 2 ;;
esac
case "$ROM_INDEX" in
  ""|*[!0-9]*) echo "error: --rom-index must be an integer: $ROM_INDEX" >&2; exit 2 ;;
esac
[ "$ROM_INDEX" -ge 1 ] || { echo "error: --rom-index must be >= 1" >&2; exit 2; }
case "$RA_MAX_FRAMES" in
  ""|*[!0-9]*) [ -z "$RA_MAX_FRAMES" ] || { echo "error: --retroarch-max-frames must be an integer: $RA_MAX_FRAMES" >&2; exit 2; } ;;
esac
case "$RA_QUIT_SEC" in
  ""|*[!0-9]*) [ -z "$RA_QUIT_SEC" ] || { echo "error: --retroarch-quit-sec must be an integer: $RA_QUIT_SEC" >&2; exit 2; } ;;
esac

case "$SYSTEM" in
  nes) TOP_DOWN=0 ;;
  psx) TOP_DOWN=3 ;;
  psp) TOP_DOWN=4 ;;
  dos) TOP_DOWN=5 ;;
  easyrpg) TOP_DOWN=6 ;;
  scummvm) TOP_DOWN=7 ;;
  *) echo "error: unsupported SYSTEM for scripted FE probe: $SYSTEM" >&2; exit 2 ;;
esac

tokens=()
for ((i = 0; i < TOP_DOWN; i++)); do tokens+=(down); done
tokens+=(a)
for ((i = 1; i < ROM_INDEX; i++)); do tokens+=(down); done
tokens+=(a q)
SCRIPT=$(IFS=,; echo "${tokens[*]}")

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}-${SYSTEM}"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/pre-stop.txt" 2>&1 || true

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "SYSTEM='$SYSTEM' RUN_SEC='$RUN_SEC' SETTLE_SEC='$SETTLE_SEC' RA_MAX_FRAMES='$RA_MAX_FRAMES' RA_QUIT_SEC='$RA_QUIT_SEC' EXPECT_EXECUTE_OK='$EXPECT_EXECUTE_OK' RESTART_FE='$RESTART_FE' SCRIPT='$SCRIPT' sh -s" <<'REMOTE' \
  >"${OUT_DIR}/remote.log" 2>&1
set -u

PLUMOS_ROOT=/mnt/SDCARD/plumos
SYSTEM=${SYSTEM:?}
RUN_SEC=${RUN_SEC:?}
SETTLE_SEC=${SETTLE_SEC:?}
RA_MAX_FRAMES=${RA_MAX_FRAMES:-}
RA_QUIT_SEC=${RA_QUIT_SEC:-}
EXPECT_EXECUTE_OK=${EXPECT_EXECUTE_OK:-0}
RESTART_FE=${RESTART_FE:?}
SCRIPT=${SCRIPT:?}
FE_LOG=/tmp/plumos-fe-launch-probe.log
FRONTEND_LAUNCH_LOG=${PLUMOS_ROOT}/logs/frontend-launch.log

cpu_line() {
  p=/sys/devices/system/cpu/cpu0/cpufreq
  printf 'gov=%s min=%s max=%s cur=%s online=%s\n' \
    "$(cat "$p/scaling_governor" 2>/dev/null || true)" \
    "$(cat "$p/scaling_min_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_max_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_cur_freq" 2>/dev/null || true)" \
    "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
}

cpu_policy_key() {
  cpu_line | sed 's/ cur=[^ ]*//'
}

interesting_ps() {
  ps w | grep -E 'plumos-controller-ui-mali|plumos-text-ui|frontend-launch|plumos-retroarch-launch|retroarch|plumos-standalone-launch|PPSSPP|pcsx|/scummvm/bin/scummvm|easyrpg-player|/dosbox|plumos-joystickd' | grep -v grep || true
}

kill_pid_quick() {
  pid=$1
  case "$pid" in
    ""|*[!0-9]*) return 0 ;;
  esac
  [ "$pid" = "$$" ] && return 0
  kill -TERM "$pid" 2>/dev/null || true
  kill -KILL "$pid" 2>/dev/null || true
}

kill_launch_targets() {
  pids=""
  for f in /proc/[0-9]*/cmdline; do
    [ -r "$f" ] || continue
    pid=${f#/proc/}
    pid=${pid%/cmdline}
    [ "$pid" = "$$" ] && continue
    cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
    case "$cmd" in
      *plumos-controller-ui-mali*) ;;
      *plumos-text-ui*) ;;
      '/bin/sh /mnt/SDCARD/plumos/bin/plumos-standalone-launch '*|\
      '/bin/sh /mnt/SDCARD/plumos/bin/plumos-retroarch-launch '*|\
      *'/mnt/SDCARD/plumos/emulators/ppsspp/'*|\
      *'/mnt/SDCARD/plumos/emulators/ppsspp-vanilla/'*|\
      *'/mnt/SDCARD/plumos/emulators/pcsx_rearmed/'*|\
      *'/mnt/SDCARD/plumos/emulators/scummvm/bin/scummvm'*|\
      *'/mnt/SDCARD/plumos/emulators/easyrpg/bin/easyrpg-player'*|\
      *'/mnt/SDCARD/plumos/emulators/dosbox-staging/'*|\
      *'/mnt/SDCARD/plumos/retroarch/bin/retroarch'*|\
      *PPSSPPSDL*|*pcsx_rearmed*|*easyrpg-player*|\
      *plumos-joystickd*)
        case " $pids " in *" $pid "*) ;; *) pids="$pids $pid" ;; esac
        ;;
    esac
  done
  for pid in $pids; do
    kill -TERM "$pid" 2>/dev/null || true
  done
  sleep 1
  for pid in $pids; do
    if kill -0 "$pid" 2>/dev/null; then
      kill_pid_quick "$pid"
    fi
  done
}

wait_for_launch_start() {
  i=0
  while [ "$i" -lt 80 ]; do
    if interesting_ps | grep -Eq 'plumos-standalone-launch|plumos-retroarch-launch|retroarch|PPSSPP|pcsx|/scummvm/bin/scummvm|easyrpg-player|/dosbox'; then
      return 0
    fi
    i=$((i + 1))
    sleep 0.25
  done
  return 1
}

wait_for_fe_exit() {
  fe_pid=$1
  i=0
  while [ "$i" -lt 80 ]; do
    kill -0 "$fe_pid" 2>/dev/null || return 0
    i=$((i + 1))
    sleep 0.25
  done
  return 1
}

restart_fe() {
  [ "$RESTART_FE" = 1 ] || return 0
  if [ -x "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" ] &&
     [ -z "$(ps w | grep -F "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" | grep -v grep || true)" ]; then
    trap '' HUP
    PLUMOS_FRONTEND_MODE=manual "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" \
      --rescue-network --rotation auto >/tmp/plumos-frontend-resume.log 2>&1 </dev/null &
    echo "frontend_restarted=1"
  fi
}

mkdir -p "$PLUMOS_ROOT/logs"
rm -f "$FE_LOG" "$FRONTEND_LAUNCH_LOG"

echo "system=$SYSTEM"
echo "script=$SCRIPT"
echo "run_sec=$RUN_SEC"
echo "settle_sec=$SETTLE_SEC"
[ -z "$RA_MAX_FRAMES" ] || echo "retroarch_max_frames=$RA_MAX_FRAMES"
[ -z "$RA_QUIT_SEC" ] || echo "retroarch_quit_sec=$RA_QUIT_SEC"
[ "$EXPECT_EXECUTE_OK" = 0 ] || echo "expect_execute_ok=1"
echo "== before =="
cpu_line
before_cpu_policy="$(cpu_policy_key)"
echo "before_cpu_policy=$before_cpu_policy"
interesting_ps

trap '' HUP
PLUMOS_SDCARD_ROOT=/mnt/SDCARD PLUMOS_ROOT="$PLUMOS_ROOT" \
  PLUMOS_RA_MAX_FRAMES="$RA_MAX_FRAMES" \
  PLUMOS_RA_QUIT_PRESS_TWICE="$([ -n "$RA_QUIT_SEC" ] && echo false || true)" \
  PLUMOS_FRONTEND_MODE=probe \
  "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" --rotation auto --script "$SCRIPT" \
  >"$FE_LOG" 2>&1 </dev/null &
FE_PID=$!
echo "fe_pid=$FE_PID"

if wait_for_launch_start; then
  echo "launch_started=yes"
else
  echo "launch_started=no"
fi
sleep "$SETTLE_SEC"
echo "== during =="
cpu_line
interesting_ps

if [ -n "$RA_QUIT_SEC" ]; then
  sleep "$RA_QUIT_SEC"
  echo "send_retroarch_quit=1"
  if [ -x "$PLUMOS_ROOT/bin/plumos-udp-send" ]; then
    "$PLUMOS_ROOT/bin/plumos-udp-send" --repeat 2 --delay-ms 500 QUIT || true
  else
    echo "warning: missing $PLUMOS_ROOT/bin/plumos-udp-send"
  fi
else
  sleep "$RUN_SEC"
  kill_launch_targets
fi

if wait_for_fe_exit "$FE_PID"; then
  echo "fe_script_exited=yes"
else
  echo "fe_script_exited=no"
  kill_pid_quick "$FE_PID"
  kill_launch_targets
fi
sleep 1

echo "== after =="
cpu_line
interesting_ps

echo "== frontend launch log tail =="
tail -n 120 "$FRONTEND_LAUNCH_LOG" 2>/dev/null || true
echo "== FE log tail =="
tail -n 80 "$FE_LOG" 2>/dev/null || true

remaining="$(interesting_ps | grep -Ev 'plumos-controller-ui-mali|ash -c' || true)"
after_cpu_policy="$(cpu_policy_key)"
echo "after_cpu_policy=$after_cpu_policy"
if [ "$after_cpu_policy" = "$before_cpu_policy" ]; then
  cpu_restored=1
else
  cpu_restored=0
fi
if [ -z "$remaining" ] && [ "$cpu_restored" = 1 ] &&
   grep -q 'can_execute: yes' "$FRONTEND_LAUNCH_LOG" 2>/dev/null; then
  probe_ok=1
else
  probe_ok=0
fi
execute_ok=0
if grep -q 'execute: ok' "$FRONTEND_LAUNCH_LOG" 2>/dev/null; then
  execute_ok=1
fi
echo "execute_ok=$execute_ok"
if [ "$EXPECT_EXECUTE_OK" = 1 ] && [ "$execute_ok" != 1 ]; then
  probe_ok=0
fi
if [ "$probe_ok" = 1 ]; then
  if [ "$EXPECT_EXECUTE_OK" = 1 ]; then
    echo "result=fe_launch_probe_execute_ok"
  else
    echo "result=fe_launch_probe_ok"
  fi
else
  echo "result=fe_launch_probe_check_logs"
fi

restart_fe
REMOTE

printf 'out_dir=%s\n' "$OUT_DIR"
cat "${OUT_DIR}/remote.log"
