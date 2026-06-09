#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
ROM="${PLUMOS_PSP_ROM:-/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso}"
RUN_MS="${PLUMOS_PPSSPP_RUN_MS:-60000}"
CPU_POLICY="${PLUMOS_A30_PSP_CPU_POLICY:-fixed}"
CPU_FREQ="${PLUMOS_A30_PSP_CPU_FREQ:-1344000}"
CPU_CORES="${PLUMOS_A30_PSP_CPU_CORES:-4}"
INPUT_TRACE="${PLUMOS_A30_INPUT_TRACE:-1}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/a30-probes/ppsspp-standalone-runtime}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--rom PATH] [--run-ms MS] [--cpu POLICY] [--freq KHZ] [--cores 2|4]

Launches PPSSPP through the standard plumOS standalone launcher and records
CPU policy, input/fd ownership, PPSSPP log snippets, and cleanup state.
While this runs, listen for audio breakup and try A/B/X/Y, D-pad, L/R,
START/SELECT, and the left stick on the A30.

Defaults:
  ROM           ${ROM}
  RUN_MS        ${RUN_MS}
  CPU_POLICY    ${CPU_POLICY}
  CPU_FREQ      ${CPU_FREQ}
  CPU_CORES     ${CPU_CORES}
  INPUT_TRACE   ${INPUT_TRACE}
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
  OUT_ROOT      ${OUT_ROOT}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --rom)
      ROM="${2:?missing --rom value}"
      shift 2
      ;;
    --run-ms)
      RUN_MS="${2:?missing --run-ms value}"
      shift 2
      ;;
    --cpu)
      CPU_POLICY="${2:?missing --cpu value}"
      shift 2
      ;;
    --freq)
      CPU_FREQ="${2:?missing --freq value}"
      shift 2
      ;;
    --cores)
      CPU_CORES="${2:?missing --cores value}"
      shift 2
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

case "$RUN_MS" in
  ""|*[!0-9]*)
    echo "error: run-ms must be an integer: $RUN_MS" >&2
    exit 2
    ;;
esac
case "$CPU_POLICY" in
  performance|fixed) ;;
  *) echo "error: invalid CPU policy: $CPU_POLICY" >&2; exit 2 ;;
esac
case "$CPU_FREQ" in
  ""|*[!0-9]*) echo "error: invalid CPU frequency: $CPU_FREQ" >&2; exit 2 ;;
esac
case "$CPU_CORES" in
  2|4) ;;
  *) echo "error: invalid CPU cores: $CPU_CORES" >&2; exit 2 ;;
esac

stamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_ROOT}/${stamp}"
mkdir -p "$OUT_DIR"

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/pre-stop.txt" || true

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "ROM='$ROM' RUN_MS='$RUN_MS' CPU_POLICY='$CPU_POLICY' CPU_FREQ='$CPU_FREQ' CPU_CORES='$CPU_CORES' INPUT_TRACE='$INPUT_TRACE' sh -s" <<'REMOTE' \
  >"${OUT_DIR}/remote.log" 2>&1
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
ROM="${ROM:?}"
RUN_MS="${RUN_MS:?}"
CPU_POLICY="${CPU_POLICY:?}"
CPU_FREQ="${CPU_FREQ:?}"
CPU_CORES="${CPU_CORES:?}"
INPUT_TRACE="${INPUT_TRACE:?}"
LAUNCH_LOG=/tmp/plumos-ppsspp-standalone-runtime.log
READER_LOG=/tmp/plumos-ppsspp-standalone-runtime-reader.log

launcher_pid=
reader_pid=

cleanup() {
  [ -n "${reader_pid:-}" ] && kill "$reader_pid" 2>/dev/null || true
  [ -n "${launcher_pid:-}" ] && kill -TERM "$launcher_pid" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

cpu_line() {
  p=/sys/devices/system/cpu/cpu0/cpufreq
  printf 'gov=%s min=%s max=%s cur=%s online=%s\n' \
    "$(cat "$p/scaling_governor" 2>/dev/null || true)" \
    "$(cat "$p/scaling_min_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_max_freq" 2>/dev/null || true)" \
    "$(cat "$p/scaling_cur_freq" 2>/dev/null || true)" \
    "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
}

fd_holders() {
  for fd in /proc/[0-9]*/fd/*; do
    target=$(readlink "$fd" 2>/dev/null) || continue
    case "$target" in
      /dev/input/*|/dev/uinput|/dev/ttyS0|/dev/fb0)
        pid=${fd#/proc/}
        pid=${pid%%/*}
        cmd=$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)
        comm=$(cat "/proc/$pid/comm" 2>/dev/null || true)
        case "$cmd $comm" in
          *plumos*|*PPSSPP*|*ld-linux-armhf*|*xpad*|*MainUI*|*keymon*)
            echo "$pid $comm $cmd $fd -> $target"
            ;;
        esac
        ;;
    esac
  done | sort
}

find_gamepad_paths() {
  event_path=""
  js_path=""
  for e in /sys/class/input/event*; do
    [ -r "$e/device/name" ] || continue
    [ "$(cat "$e/device/name")" = "plumOS A30 Gamepad" ] || continue
    event_path="/dev/input/${e##*/}"
  done
  for j in /sys/class/input/js*; do
    [ -r "$j/device/name" ] || continue
    [ "$(cat "$j/device/name")" = "plumOS A30 Gamepad" ] || continue
    js_path="/dev/input/${j##*/}"
  done
  printf '%s %s\n' "$js_path" "$event_path"
}

echo "rom=$ROM"
echo "run_ms=$RUN_MS"
echo "cpu_policy=$CPU_POLICY"
echo "cpu_freq=$CPU_FREQ"
echo "cpu_cores=$CPU_CORES"
echo "input_trace=$INPUT_TRACE"

[ -f "$ROM" ] || {
  echo "result=missing_rom"
  exit 2
}

rm -f "$LAUNCH_LOG" "$READER_LOG"

echo "== cpu before =="
cpu_line

env \
  PLUMOS_STANDALONE_USE_STOCK_SDL=1 \
  PLUMOS_A30_PSP_CPU_POLICY="$CPU_POLICY" \
  PLUMOS_A30_PSP_CPU_FREQ="$CPU_FREQ" \
  PLUMOS_A30_PSP_CPU_CORES="$CPU_CORES" \
  PLUMOS_A30_INPUT_TRACE="$INPUT_TRACE" \
  "$PLUMOS_ROOT/bin/plumos-standalone-launch" ppsspp "$ROM" >"$LAUNCH_LOG" 2>&1 &
launcher_pid=$!
echo "launcher_pid=$launcher_pid"

sleep 5

echo "== cpu during =="
cpu_line

echo "== fd holders during =="
fd_holders

set -- $(find_gamepad_paths)
js_path=${1:-}
event_path=${2:-}
echo "detected_gamepad js=$js_path event=$event_path"
cat /proc/bus/input/devices | sed -n "/Name=\"plumOS A30 Gamepad\"/,+8p" || true

if [ -n "$js_path" ] && [ -n "$event_path" ]; then
  "$PLUMOS_ROOT/bin/plumos-joystick-reader" \
    --js "$js_path" \
    --event "$event_path" \
    --name "plumOS A30 Gamepad" \
    --timeout-ms "$RUN_MS" \
    >"$READER_LOG" 2>&1 &
  reader_pid=$!
else
  echo "result=gamepad_missing" >"$READER_LOG"
fi

sleep_sec=$((RUN_MS / 1000))
[ "$sleep_sec" -gt 0 ] || sleep_sec=1
sleep "$sleep_sec"

if [ -n "${reader_pid:-}" ]; then
  wait "$reader_pid" 2>/dev/null || true
fi

echo "== cpu before stop =="
cpu_line

kill -TERM "$launcher_pid" 2>/dev/null || true
sleep 4
launcher_pid=

echo "== cpu after =="
cpu_line

echo "== ps after =="
ps w | grep -E "plumos-standalone-launch|PPSSPPSDL|ld-linux-armhf|plumos-joystickd" | grep -v grep || true

echo "== joystickd log =="
cat "$PLUMOS_ROOT/logs/standalone/ppsspp-joystickd.log" 2>/dev/null || true

echo "== reader summary =="
grep -E "summary|type_name=JS_BUTTON|type_name=JS_AXIS|type_name=EV_KEY|type_name=EV_ABS" "$READER_LOG" 2>/dev/null | tail -n 80 || true

echo "== ppsspp log highlights =="
grep -E "SDL version|A30 display|plumOS-input|found control pad|mapping is|pad 1|BOOT|underrun|snd_pcm|Leaving main|CreateWindow failed" "$LAUNCH_LOG" || true

if grep -qi "underrun" "$LAUNCH_LOG"; then
  echo "result=audio_underrun_logged"
elif grep -q "summary js_events=[1-9]" "$READER_LOG"; then
  echo "result=runtime_no_underrun_logged_with_input"
else
  echo "result=runtime_no_underrun_logged"
fi

rm -f "$LAUNCH_LOG" "$READER_LOG"
REMOTE

"${ROOT_DIR}/scripts/stop-a30-display-processes.sh" >"${OUT_DIR}/post-stop.txt" || true

printf 'out_dir=%s\n' "$OUT_DIR"
cat "${OUT_DIR}/remote.log"
cat "${OUT_DIR}/post-stop.txt"
