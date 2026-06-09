#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)
DURATION=30
DEPLOY=0
RESTART_FE=1
ROTATION=ccw
AUDIO=oss
SYSTEM=nes
INPUT=sdl2-joypad
CPU_POLICY=fixed
CPU_FREQ=648000
CPU_CORES=2
AUDIO_LATENCY=128
VIDEO_THREADED=false

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Runs the practical plumOS RetroArch runtime on the A30 for audio/controller
testing. The probe stops the plumOS FE while RetroArch owns the framebuffer,
starts plumos-joystickd --device-mode xbox, and restarts the FE afterward.

Options:
  --deploy             Deploy dist/plumos-retroarch-practical before probing.
  --duration SECONDS   Runtime before killing RetroArch. Default: ${DURATION}
  --audio oss|alsa     Audio driver config. Default: ${AUDIO}
  --input sdl2-joypad|sdl2|udev-joypad|udev|linuxraw|null
                      Input driver config. Default: ${INPUT}
  --cpu performance|fixed
                      CPU policy during the probe. Default: ${CPU_POLICY}
  --freq KHZ          Fixed CPU frequency in kHz when --cpu fixed. Default: ${CPU_FREQ}
  --cores 2|4         CPU online core policy. Default: ${CPU_CORES}
  --latency FRAMES    RetroArch audio_latency. Default: ${AUDIO_LATENCY}
  --video-threaded true|false
                      RetroArch video_threaded setting. Default: ${VIDEO_THREADED}
  --system nes|gb      Core/ROM pair to run. Default: ${SYSTEM}
  --rotation VALUE     PLUMOS_RA_DISPLAY_ROTATION value. Default: ${ROTATION}
  --no-restart-fe      Leave the plumOS FE stopped after the probe.
  -h, --help           Show this help.
EOF
}

shell_quote() {
  local value="$1"
  printf "'%s'" "${value//\'/\'\\\'\'}"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      ;;
    --duration)
      DURATION="${2:?missing duration}"
      shift
      ;;
    --audio)
      AUDIO="${2:?missing audio driver}"
      shift
      ;;
    --input)
      INPUT="${2:?missing input driver}"
      shift
      ;;
    --cpu)
      CPU_POLICY="${2:?missing CPU policy}"
      shift
      ;;
    --freq)
      CPU_FREQ="${2:?missing CPU frequency}"
      shift
      ;;
    --cores)
      CPU_CORES="${2:?missing CPU core count}"
      shift
      ;;
    --latency)
      AUDIO_LATENCY="${2:?missing audio latency}"
      shift
      ;;
    --video-threaded)
      VIDEO_THREADED="${2:?missing video threaded setting}"
      shift
      ;;
    --system)
      SYSTEM="${2:?missing system}"
      shift
      ;;
    --rotation)
      ROTATION="${2:?missing rotation}"
      shift
      ;;
    --no-restart-fe)
      RESTART_FE=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

case "$DURATION" in
  ''|*[!0-9]*)
    echo "error: --duration must be an integer number of seconds" >&2
    exit 2
    ;;
esac
case "$AUDIO_LATENCY" in
  ''|*[!0-9]*)
    echo "error: --latency must be an integer number of frames" >&2
    exit 2
    ;;
esac
case "$CPU_FREQ" in
  ''|*[!0-9]*)
    echo "error: --freq must be an integer kHz value" >&2
    exit 2
    ;;
esac
case "$VIDEO_THREADED" in
  true|false) ;;
  *)
    echo "error: --video-threaded must be true or false" >&2
    exit 2
    ;;
esac
case "$AUDIO" in
  oss|alsa) ;;
  *)
    echo "error: --audio must be oss or alsa" >&2
    exit 2
    ;;
esac
case "$INPUT" in
  sdl2-joypad|sdl2|udev-joypad|udev|linuxraw|null) ;;
  *)
    echo "error: --input must be sdl2-joypad, sdl2, udev-joypad, udev, linuxraw, or null" >&2
    exit 2
    ;;
esac
case "$CPU_POLICY" in
  performance|fixed) ;;
  *)
    echo "error: --cpu must be performance or fixed" >&2
    exit 2
    ;;
esac
case "$CPU_CORES" in
  2|4) ;;
  *)
    echo "error: --cores must be 2 or 4" >&2
    exit 2
    ;;
esac
case "$SYSTEM" in
  nes|gb) ;;
  *)
    echo "error: --system must be nes or gb" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-retroarch-practical" /mnt/SDCARD
fi

remote_script='
set -eu

PLUMOS_ROOT=/mnt/SDCARD/plumos
RA="$PLUMOS_ROOT/bin/plumos-retroarch-practical"
FE="$PLUMOS_ROOT/bin/plumos-controller-ui-mali"
JOY="$PLUMOS_ROOT/bin/plumos-joystickd"
LOG_DIR="$PLUMOS_ROOT/retroarch/logs"
RA_LOG=/tmp/plumos-retroarch-practical.log
JOY_LOG=/tmp/plumos-retroarch-practical-joystickd.log
APPEND=/tmp/plumos-retroarch-practical-append.cfg
CPU_STATE=/tmp/plumos-retroarch-practical-cpustate
RA_PID=""
JOY_PID=""
GAMEPAD_EVENT=""
GAMEPAD_JS=""

proc_cmdline() {
  pid="$1"
  tr "\000" " " <"/proc/$pid/cmdline" 2>/dev/null || true
}

proc_pids_matching() {
  pattern="$1"
  for proc in /proc/[0-9]*; do
    [ -d "$proc" ] || continue
    pid="${proc##*/}"
    [ "$pid" = "$$" ] && continue
    cmd="$(proc_cmdline "$pid")"
    [ -n "$cmd" ] || continue
    case "$cmd" in
      *"$pattern"*) printf "%s\n" "$pid" ;;
    esac
  done
}

fe_pids() {
  {
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-controller-ui-mali" || true
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-controller-ui-mali.bin" || true
  } | awk "!seen[\$0]++"
}

retroarch_pids() {
  {
    proc_pids_matching "$PLUMOS_ROOT/bin/plumos-retroarch-practical" || true
    proc_pids_matching "$PLUMOS_ROOT/retroarch/bin/retroarch" || true
    proc_pids_matching "$PLUMOS_ROOT/retroarch/bin/retroarch.bin" || true
  } | awk "!seen[\$0]++"
}

joystickd_pids() {
  proc_pids_matching "$PLUMOS_ROOT/bin/plumos-joystickd" || true
}

kill_pids() {
  pids="$1"
  [ -n "$pids" ] || return 0
  kill $pids 2>/dev/null || true
  sleep 1
  kill -KILL $pids 2>/dev/null || true
}

cleanup() {
  if [ -n "$RA_PID" ] && kill -0 "$RA_PID" 2>/dev/null; then
    kill "$RA_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$RA_PID" 2>/dev/null || true
  fi
  if [ -n "$JOY_PID" ] && kill -0 "$JOY_PID" 2>/dev/null; then
    kill "$JOY_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$JOY_PID" 2>/dev/null || true
  fi

  cp "$RA_LOG" "$LOG_DIR/practical-last.log" 2>/dev/null || true
  cp "$JOY_LOG" "$LOG_DIR/practical-joystickd-last.log" 2>/dev/null || true

  restore_cpu_policy

  if [ "$PLUMOS_RETROARCH_PRACTICAL_RESTART_FE" = "1" ] &&
     [ -x "$FE" ] &&
     [ -z "$(fe_pids)" ]; then
    echo "== restart plumOS FE =="
    PLUMOS_FRONTEND_MODE=manual "$FE" --rotation auto >/tmp/plumos-frontend-resume.log 2>&1 </dev/null &
  fi
}

first_rom() {
  dir="$1"
  pattern="$2"
  find "$dir" -maxdepth 2 -type f -name "$pattern" 2>/dev/null |
    grep -v "/\\._" |
    head -n 1
}

input_device_name() {
  dev="$1"
  base="${dev##*/}"
  cat "/sys/class/input/$base/device/name" 2>/dev/null || true
}

find_gamepad_path() {
  kind="$1"
  for dev in /dev/input/"$kind"*; do
    [ -e "$dev" ] || continue
    if [ "$(input_device_name "$dev")" = "plumOS A30 Gamepad" ]; then
      echo "$dev"
      return 0
    fi
  done
  return 1
}

wait_for_gamepad() {
  i=0
  while [ "$i" -lt 30 ]; do
    GAMEPAD_EVENT="$(find_gamepad_path event || true)"
    GAMEPAD_JS="$(find_gamepad_path js || true)"
    if [ -n "$GAMEPAD_EVENT" ] || [ -n "$GAMEPAD_JS" ]; then
      echo "event=${GAMEPAD_EVENT:-} js=${GAMEPAD_JS:-}"
      return 0
    fi
    i=$((i + 1))
    sleep 0.1
  done
  return 1
}

save_cpu_policy() {
  {
    printf "cpu_online=%s\n" "$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
    printf "governor=%s\n" "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true)"
    printf "min_freq=%s\n" "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq 2>/dev/null || true)"
    printf "max_freq=%s\n" "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || true)"
    printf "setspeed=%s\n" "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2>/dev/null || true)"
    for cpu in 1 2 3; do
      if [ -r "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
        printf "cpu%s_online=%s\n" "$cpu" "$(cat "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true)"
      fi
    done
  } > "$CPU_STATE"
}

restore_cpu_policy() {
  [ -f "$CPU_STATE" ] || return 0
  # shellcheck disable=SC1090
  . "$CPU_STATE" 2>/dev/null || true
  for cpu in 1 2 3; do
    eval value="\${cpu${cpu}_online:-}"
    if [ -n "$value" ] && [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
      echo "$value" > "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
    fi
  done
  p=/sys/devices/system/cpu/cpu0/cpufreq
  if [ -n "${max_freq:-}" ] && [ -w "$p/scaling_max_freq" ]; then
    echo "$max_freq" > "$p/scaling_max_freq" 2>/dev/null || true
  fi
  if [ -n "${min_freq:-}" ] && [ -w "$p/scaling_min_freq" ]; then
    echo "$min_freq" > "$p/scaling_min_freq" 2>/dev/null || true
  fi
  if [ -n "${governor:-}" ] && [ -w "$p/scaling_governor" ]; then
    echo "$governor" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
  fi
  rm -f "$CPU_STATE"
}

apply_cpu_policy() {
  save_cpu_policy
  case "$PLUMOS_RETROARCH_PRACTICAL_CPU_CORES" in
    2)
      [ -w /sys/devices/system/cpu/cpu1/online ] &&
        echo 1 > /sys/devices/system/cpu/cpu1/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu2/online ] &&
        echo 0 > /sys/devices/system/cpu/cpu2/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu3/online ] &&
        echo 0 > /sys/devices/system/cpu/cpu3/online 2>/dev/null || true
      ;;
    4)
      for cpu in 1 2 3; do
        [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ] &&
          echo 1 > "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
      done
      ;;
  esac
  case "$PLUMOS_RETROARCH_PRACTICAL_CPU_POLICY" in
    performance)
      echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
      ;;
    fixed)
      p=/sys/devices/system/cpu/cpu0/cpufreq
      freq="$PLUMOS_RETROARCH_PRACTICAL_CPU_FREQ"
      echo userspace > "$p/scaling_governor" 2>/dev/null || true
      echo "$freq" > "$p/scaling_setspeed" 2>/dev/null || true
      echo "$freq" > "$p/scaling_min_freq" 2>/dev/null || true
      echo "$freq" > "$p/scaling_max_freq" 2>/dev/null || true
      ;;
  esac
}

print_cpu_policy() {
  echo "online=$(cat /sys/devices/system/cpu/online 2>/dev/null || true)"
  p=/sys/devices/system/cpu/cpu0/cpufreq
  for f in scaling_governor scaling_cur_freq scaling_min_freq scaling_max_freq cpuinfo_max_freq; do
    [ -r "$p/$f" ] && printf "%s=%s\n" "$f" "$(cat "$p/$f")"
  done
}

trap cleanup EXIT
mkdir -p "$LOG_DIR"
rm -f "$RA_LOG" "$JOY_LOG"

case "$PLUMOS_RETROARCH_PRACTICAL_SYSTEM" in
  nes)
    CORE="$PLUMOS_ROOT/retroarch/cores/fceumm_libretro.so"
    ROM="$(first_rom /mnt/SDCARD/Roms/FC "*.nes")"
    ;;
  gb)
    CORE="$PLUMOS_ROOT/retroarch/cores/gambatte_libretro.so"
    ROM="$(first_rom /mnt/SDCARD/Roms/GB "*.gb")"
    ;;
esac

if [ -z "$ROM" ]; then
  echo "error: no ROM found for system $PLUMOS_RETROARCH_PRACTICAL_SYSTEM" >&2
  exit 1
fi

cat > "$APPEND" <<EOF
video_rotation = "1"
screen_orientation = "1"
config_save_on_exit = "false"
audio_latency = "$PLUMOS_RETROARCH_PRACTICAL_AUDIO_LATENCY"
video_threaded = "$PLUMOS_RETROARCH_PRACTICAL_VIDEO_THREADED"
notification_show_autoconfig = "true"
EOF

case "$PLUMOS_RETROARCH_PRACTICAL_INPUT" in
  sdl2-joypad)
    cat >> "$APPEND" <<EOF
input_driver = "null"
input_joypad_driver = "sdl2"
EOF
    ;;
  sdl2)
    cat >> "$APPEND" <<EOF
input_driver = "sdl2"
input_joypad_driver = "sdl2"
EOF
    ;;
  udev-joypad)
    cat >> "$APPEND" <<EOF
input_driver = "null"
input_joypad_driver = "udev"
EOF
    ;;
  udev|linuxraw|null)
    cat >> "$APPEND" <<EOF
input_driver = "$PLUMOS_RETROARCH_PRACTICAL_INPUT"
input_joypad_driver = "$PLUMOS_RETROARCH_PRACTICAL_INPUT"
EOF
    ;;
esac

if [ "$PLUMOS_RETROARCH_PRACTICAL_AUDIO" = "alsa" ]; then
  cat >> "$APPEND" <<EOF
audio_driver = "alsa"
audio_device = "hw:0,0"
EOF
fi

echo "== binaries =="
ls -l "$RA" "$CORE" "$JOY" "$PLUMOS_ROOT/retroarch/config/retroarch-practical.cfg" 2>/dev/null || true
echo "== audio devices =="
ls -l /dev/dsp /dev/mixer /dev/snd/* 2>/dev/null || true
echo "== cpu policy before =="
print_cpu_policy
echo "== input devices before =="
for dev in /dev/input/event* /dev/input/js*; do
  [ -e "$dev" ] || continue
  printf "%s name=%s\n" "$dev" "$(input_device_name "$dev")"
done
echo "== display/input processes before =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch|plumos-joystickd" | grep -v grep || true

old_ra_pids="$(retroarch_pids)"
[ -n "$old_ra_pids" ] && kill_pids "$old_ra_pids"
old_joy_pids="$(joystickd_pids)"
[ -n "$old_joy_pids" ] && kill_pids "$old_joy_pids"

echo "== apply cpu policy =="
apply_cpu_policy
print_cpu_policy

fe_pids_before="$(fe_pids)"
if [ -n "$fe_pids_before" ]; then
  echo "== stop plumOS FE for framebuffer ownership =="
  kill_pids "$fe_pids_before"
fi

echo "== start joystickd =="
"$JOY" --device-mode xbox --timeout-ms "$((PLUMOS_RETROARCH_PRACTICAL_DURATION * 1000 + 8000))" --print-every 120 >"$JOY_LOG" 2>&1 &
JOY_PID=$!
if js_path="$(wait_for_gamepad)"; then
  echo "gamepad=$js_path"
else
  echo "warning: plumOS A30 Gamepad did not appear before RetroArch start"
fi

echo "== run RetroArch practical =="
echo "system=$PLUMOS_RETROARCH_PRACTICAL_SYSTEM audio=$PLUMOS_RETROARCH_PRACTICAL_AUDIO input=$PLUMOS_RETROARCH_PRACTICAL_INPUT cpu=$PLUMOS_RETROARCH_PRACTICAL_CPU_POLICY freq=$PLUMOS_RETROARCH_PRACTICAL_CPU_FREQ cores=$PLUMOS_RETROARCH_PRACTICAL_CPU_CORES latency=$PLUMOS_RETROARCH_PRACTICAL_AUDIO_LATENCY video_threaded=$PLUMOS_RETROARCH_PRACTICAL_VIDEO_THREADED duration=${PLUMOS_RETROARCH_PRACTICAL_DURATION}s"
echo "core=$CORE"
echo "rom=$ROM"
set +e
if [ -n "$GAMEPAD_JS" ]; then
  SDL_JOYSTICK_DEVICE="$GAMEPAD_JS" \
  PLUMOS_RA_DISPLAY_ROTATION="$PLUMOS_RETROARCH_PRACTICAL_ROTATION" \
    "$RA" --appendconfig "$APPEND" -L "$CORE" "$ROM" >"$RA_LOG" 2>&1 &
else
  PLUMOS_RA_DISPLAY_ROTATION="$PLUMOS_RETROARCH_PRACTICAL_ROTATION" \
  "$RA" --appendconfig "$APPEND" -L "$CORE" "$ROM" >"$RA_LOG" 2>&1 &
fi
RA_PID=$!
sleep "$PLUMOS_RETROARCH_PRACTICAL_DURATION"
if kill -0 "$RA_PID" 2>/dev/null; then
  kill "$RA_PID" 2>/dev/null || true
  sleep 1
  wait "$RA_PID" 2>/dev/null
  wait_rc=$?
  survived=1
else
  wait "$RA_PID" 2>/dev/null
  wait_rc=$?
  survived=0
fi
set -e
RA_PID=""

if [ -n "$JOY_PID" ] && kill -0 "$JOY_PID" 2>/dev/null; then
  kill "$JOY_PID" 2>/dev/null || true
  sleep 1
  wait "$JOY_PID" 2>/dev/null || true
fi
JOY_PID=""

cp "$RA_LOG" "$LOG_DIR/practical-last.log" 2>/dev/null || true
cp "$JOY_LOG" "$LOG_DIR/practical-joystickd-last.log" 2>/dev/null || true

echo "== retroarch log highlights =="
grep -Ei "RetroArch|Core|Content|Audio|audio|OSS|ALSA|SDL|dsp|pcm|Input|Joypad|LinuxRaw|autoconfig|configured|gamepad|controller|video|EGL|GLES|Mali|error|failed|fatal|segmentation" "$RA_LOG" |
  tail -n 180 || true
echo "== joystickd log tail =="
tail -n 80 "$JOY_LOG" || true
echo "== display/input processes after =="
ps w 2>/dev/null | grep -E "MainUI|keymon|plumos-controller-ui-mali|retroarch|plumos-joystickd" | grep -v grep || true

if [ "$survived" = "1" ]; then
  echo "result=retroarch_practical_survived_${PLUMOS_RETROARCH_PRACTICAL_DURATION}s"
  exit 0
fi

echo "result=retroarch_practical_exited_rc_${wait_rc}"
exit "$wait_rc"
'

remote_env_cmd="env"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_DURATION=$(shell_quote "$DURATION")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_RESTART_FE=$(shell_quote "$RESTART_FE")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_ROTATION=$(shell_quote "$ROTATION")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_AUDIO=$(shell_quote "$AUDIO")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_INPUT=$(shell_quote "$INPUT")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_CPU_POLICY=$(shell_quote "$CPU_POLICY")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_CPU_FREQ=$(shell_quote "$CPU_FREQ")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_CPU_CORES=$(shell_quote "$CPU_CORES")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_AUDIO_LATENCY=$(shell_quote "$AUDIO_LATENCY")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_VIDEO_THREADED=$(shell_quote "$VIDEO_THREADED")"
remote_env_cmd="$remote_env_cmd PLUMOS_RETROARCH_PRACTICAL_SYSTEM=$(shell_quote "$SYSTEM")"
remote_env_cmd="$remote_env_cmd sh -s"

printf '%s\n' "$remote_script" | ssh "${SSH_OPTS[@]}" "$TARGET" "$remote_env_cmd"
