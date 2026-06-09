#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SYSTEM="${1:-nes}"
TRIGGER=signal
TIMEOUT_SEC=45
RESTART_FE=1
SAFE_STATE_SLOT="${PLUMOS_RA_SAFE_STATE_SLOT:-999}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/safe-shutdown}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [SYSTEM] [options]

Launches a ROM through plumos-text-ui --execute, verifies that text-ui
auto-starts plumos-safe-hotkeyd, then triggers the hotkey path.

SYSTEM:
  nes

Options:
  --trigger signal      Send SIGUSR1 to the auto-started hotkeyd. Default.
  --trigger physical    Wait for a physical Function button press.
  --safe-state-slot N   RetroArch safe shutdown/resume savestate slot.
                        Default: ${SAFE_STATE_SLOT}
  --timeout-sec SEC     Maximum seconds to wait after launch. Default: ${TIMEOUT_SEC}
  --no-restart-fe       Leave the FE stopped after the probe.
  -h, --help            Show this help.

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
    --trigger)
      TRIGGER="${2:?missing --trigger value}"
      shift 2
      ;;
    --timeout-sec)
      TIMEOUT_SEC="${2:?missing --timeout-sec value}"
      shift 2
      ;;
    --safe-state-slot)
      SAFE_STATE_SLOT="${2:?missing --safe-state-slot value}"
      shift 2
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
  nes) ;;
  *) echo "error: unsupported SYSTEM: $SYSTEM" >&2; exit 2 ;;
esac
case "$TRIGGER" in
  signal|physical) ;;
  *) echo "error: --trigger must be signal or physical: $TRIGGER" >&2; exit 2 ;;
esac
case "$TIMEOUT_SEC" in
  ""|*[!0-9]*) echo "error: --timeout-sec must be an integer: $TIMEOUT_SEC" >&2; exit 2 ;;
esac
case "$SAFE_STATE_SLOT" in
  0|[1-9]|[1-9][0-9]|[1-9][0-9][0-9]) ;;
  *)
    echo "error: --safe-state-slot must be 0..999 without leading zeroes: $SAFE_STATE_SLOT" >&2
    exit 2
    ;;
esac

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}-text-ui-autohotkey-${TRIGGER}-${SYSTEM}"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/pre-stop.txt" 2>&1 || true

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "SYSTEM='$SYSTEM' TRIGGER='$TRIGGER' TIMEOUT_SEC='$TIMEOUT_SEC' RESTART_FE='$RESTART_FE' SAFE_STATE_SLOT='$SAFE_STATE_SLOT' sh -s" <<'REMOTE' \
  >"${OUT_DIR}/remote.log" 2>&1
set -u

PLUMOS_ROOT=/mnt/SDCARD/plumos
SYSTEM=${SYSTEM:?}
TRIGGER=${TRIGGER:?}
TIMEOUT_SEC=${TIMEOUT_SEC:?}
RESTART_FE=${RESTART_FE:-1}
SAFE_STATE_SLOT=${SAFE_STATE_SLOT:?}
TEXTUI_LOG=/tmp/plumos-safe-hotkeyd-probe-textui.log
FRONTEND_LOG=/tmp/plumos-frontend-resume.log
SELF=$$
TEXTUI_PID=
RESULT=fail

case "$SYSTEM" in
  nes)
    ROM='FC/Legend of Zelda, The (USA) (Rev 1).nes'
    ;;
  *)
    echo "error=unsupported_system system=$SYSTEM"
    exit 2
    ;;
esac

find_pids() {
  pattern=$1
  for d in /proc/[0-9]*; do
    [ -r "$d/cmdline" ] || continue
    pid=${d#/proc/}
    [ "$pid" = "$SELF" ] && continue
    cmd=$(tr '\000' ' ' <"$d/cmdline" 2>/dev/null || true)
    case "$cmd" in
      *"$pattern"*) printf '%s ' "$pid" ;;
    esac
  done
}

find_boot_mainui_pids() {
  for d in /proc/[0-9]*; do
    [ -r "$d/cmdline" ] || continue
    pid=${d#/proc/}
    [ "$pid" = "$SELF" ] && continue
    cmd=$(tr '\000' ' ' <"$d/cmdline" 2>/dev/null || true)
    case "$cmd" in
      *'/etc/main '*|\
      *'/mnt/SDCARD/miyoo/app/MainUI '*|\
      *'/mnt/SDCARD/plumos/bootstrap/MainUI.wrapper '*|\
      *'/mnt/SDCARD/miyoo/app/MainUI.stock '*)
        printf '%s ' "$pid"
        ;;
    esac
  done
}

stop_boot_mainui() {
  reason=$1
  pids=$(find_boot_mainui_pids)
  if [ -n "$pids" ]; then
    echo "stop_boot_mainui_${reason}=$pids"
    kill -TERM $pids >/dev/null 2>&1 || true
    sleep 1
    kill -KILL $pids >/dev/null 2>&1 || true
  else
    echo "stop_boot_mainui_${reason}=none"
  fi
}

wait_for_pattern() {
  pattern=$1
  limit=$2
  i=0
  while [ "$i" -lt "$limit" ]; do
    pids=$(find_pids "$pattern")
    if [ -n "$pids" ]; then
      printf '%s' "$pids"
      return 0
    fi
    sleep 1
    i=$((i + 1))
  done
  return 1
}

cpu_line() {
  p=/sys/devices/system/cpu/cpu0/cpufreq
  printf 'gov=%s min=%s max=%s cur=%s online=%s\n' \
    "$(cat "$p/scaling_governor" 2>/dev/null || true)" \
    "$(cat "$p/scaling_min_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_max_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_cur_freq" 2>/dev/null || true)" \
    "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
}

safe_state_files_for_slot() {
  rom_base=${ROM##*/}
  rom_stem=${rom_base%.*}
  find "$PLUMOS_ROOT/retroarch/states" -type f -name "*${rom_stem}*state${SAFE_STATE_SLOT}*" 2>/dev/null || true
}

restart_fe() {
  [ "$RESTART_FE" = 1 ] || return 0
  stop_boot_mainui restart_fe
  if [ -x "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" ]; then
    pids=$(find_pids 'plumos-controller-ui-mali.bin')
    if [ -z "$pids" ]; then
      trap '' HUP
      PLUMOS_FRONTEND_MODE=manual "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" \
        --rescue-network --rotation auto >"$FRONTEND_LOG" 2>&1 </dev/null &
      echo "frontend_restart_pid=$!"
    else
      echo "frontend_already_running=$pids"
    fi
  fi
}

cleanup() {
  rc=$?
  hot=$(find_pids 'plumos-safe-hotkeyd')
  [ -n "$hot" ] && kill $hot >/dev/null 2>&1 || true
  if [ -n "${TEXTUI_PID:-}" ] && kill -0 "$TEXTUI_PID" >/dev/null 2>&1; then
    "$PLUMOS_ROOT/bin/plumos-safe-shutdown" --shutdown --no-poweroff \
      >/tmp/plumos-safe-hotkeyd-probe-cleanup.log 2>&1 || true
    kill "$TEXTUI_PID" >/dev/null 2>&1 || true
  fi
  restart_fe
  exit "$rc"
}
trap cleanup EXIT INT TERM

rm -f "$TEXTUI_LOG" \
  "$PLUMOS_ROOT/logs/safe-hotkeyd.log" \
  "$PLUMOS_ROOT/logs/safe-hotkeyd-command.log" \
  "$PLUMOS_ROOT/logs/safe-shutdown.log"

echo "test=plumos_safe_hotkeyd_probe"
date '+remote_time=%Y-%m-%dT%H:%M:%S%z' 2>/dev/null || true
echo "system=$SYSTEM"
echo "rom=$ROM"
echo "trigger=$TRIGGER"
echo "timeout_sec=$TIMEOUT_SEC"
echo "safe_state_slot=$SAFE_STATE_SLOT"
echo "== before =="
cpu_line
stop_boot_mainui pre_launch

PLUMOS_RA_SAFE_STATE_SLOT="$SAFE_STATE_SLOT" \
  "$PLUMOS_ROOT/bin/plumos-text-ui" launch "$SYSTEM" "$ROM" --execute --no-scan \
  >"$TEXTUI_LOG" 2>&1 &
TEXTUI_PID=$!
echo "textui_pid=$TEXTUI_PID"

launcher_pids=$(wait_for_pattern 'plumos-retroarch-launch' 25) || launcher_pids=
retroarch_pids=$(wait_for_pattern 'retroarch.bin' 25) || retroarch_pids=
hotkey_pids=$(wait_for_pattern 'plumos-safe-hotkeyd' 10) || hotkey_pids=
echo "launcher_pids=$launcher_pids"
echo "retroarch_pids=$retroarch_pids"
echo "hotkey_pids=$hotkey_pids"

if [ -z "$launcher_pids" ] || [ -z "$hotkey_pids" ]; then
  echo "error=missing_launcher_or_hotkeyd"
  cat "$TEXTUI_LOG" 2>/dev/null || true
  exit 1
fi

set -- $hotkey_pids
HOTKEY_PID=$1
if [ "$TRIGGER" = signal ]; then
  sleep 3
  kill -USR1 "$HOTKEY_PID" || true
else
  echo "action=press_physical_function_now"
fi

i=0
while kill -0 "$TEXTUI_PID" >/dev/null 2>&1; do
  if [ "$i" -ge "$TIMEOUT_SEC" ]; then
    echo "error=textui_timeout"
    break
  fi
  sleep 1
  i=$((i + 1))
done

if kill -0 "$TEXTUI_PID" >/dev/null 2>&1; then
  "$PLUMOS_ROOT/bin/plumos-safe-shutdown" --shutdown --no-poweroff \
    >/tmp/plumos-safe-hotkeyd-probe-timeout-safe-shutdown.log 2>&1 || true
  kill "$TEXTUI_PID" >/dev/null 2>&1 || true
  TEXTUI_RC=124
else
  wait "$TEXTUI_PID"
  TEXTUI_RC=$?
fi
echo "textui_rc=$TEXTUI_RC"
TEXTUI_PID=
sleep 2

p_hotkey=$(find_pids 'plumos-safe-hotkeyd')
p_textui=$(find_pids 'plumos-text-ui')
p_launcher=$(find_pids 'plumos-retroarch-launch')
p_ra=$(find_pids 'retroarch.bin')
p_joy=$(find_pids 'plumos-joystickd')
p_safe=$(find_pids 'plumos-safe-shutdown')
p_mainui=$(find_boot_mainui_pids)
safe_state_files=$(safe_state_files_for_slot | tr '\n' ' ')
echo "residual_hotkey=$p_hotkey"
echo "residual_textui=$p_textui"
echo "residual_launcher=$p_launcher"
echo "residual_retroarch=$p_ra"
echo "residual_joystickd=$p_joy"
echo "residual_safe_shutdown=$p_safe"
echo "residual_boot_mainui=$p_mainui"
echo "safe_state_files=$safe_state_files"
[ -d /tmp/plumos-safe-shutdown.lock ] && echo "safe_shutdown_lock=present" || echo "safe_shutdown_lock=none"

echo "== text-ui stdout =="
cat "$TEXTUI_LOG" 2>/dev/null || true
echo "== safe-hotkeyd log =="
cat "$PLUMOS_ROOT/logs/safe-hotkeyd.log" 2>/dev/null || true
echo "== safe-hotkeyd command log =="
cat "$PLUMOS_ROOT/logs/safe-hotkeyd-command.log" 2>/dev/null || true
echo "== safe-shutdown log =="
cat "$PLUMOS_ROOT/logs/safe-shutdown.log" 2>/dev/null || true
echo "== resume-session =="
cat "$PLUMOS_ROOT/state/frontend/resume-session.json" 2>/dev/null || true
echo "== safe-state slot files =="
safe_state_files_for_slot | while IFS= read -r path; do
  ls -l "$path" 2>/dev/null || true
done
echo "== after =="
cpu_line

if grep -q 'safe_hotkeyd: started pid=' "$TEXTUI_LOG" 2>/dev/null &&
   grep -q "safe_state_slot: $SAFE_STATE_SLOT" "$TEXTUI_LOG" 2>/dev/null &&
   grep -q 'result=plumos_safe_hotkeyd_trigger .* rc=0' "$PLUMOS_ROOT/logs/safe-hotkeyd.log" 2>/dev/null &&
   grep -q 'result=plumos_safe_shutdown_ok action=shutdown poweroff=0' "$PLUMOS_ROOT/logs/safe-hotkeyd-command.log" 2>/dev/null &&
   grep -q 'execute: safe-exit' "$TEXTUI_LOG" 2>/dev/null &&
   grep -q '"pending"[[:space:]]*:[[:space:]]*true' "$PLUMOS_ROOT/state/frontend/resume-session.json" 2>/dev/null &&
   [ -n "$safe_state_files" ] &&
   [ -z "$p_hotkey$p_launcher$p_ra$p_joy$p_safe$p_mainui" ]; then
  RESULT=ok
fi
echo "result=plumos_safe_hotkeyd_${TRIGGER}_${SYSTEM}_${RESULT}"
[ "$RESULT" = ok ]
REMOTE

echo "Artifact: ${OUT_DIR}"
if grep -q "result=plumos_safe_hotkeyd_${TRIGGER}_${SYSTEM}_ok" "${OUT_DIR}/remote.log"; then
  echo "Result: ok"
else
  echo "Result: failed"
  tail -n 80 "${OUT_DIR}/remote.log" >&2 || true
  exit 1
fi
