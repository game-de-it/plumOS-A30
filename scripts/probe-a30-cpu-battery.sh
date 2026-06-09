#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)
DURATION=120
SAMPLE_INTERVAL=5
COOLDOWN=20
ALLOW_EXTERNAL_POWER=0
ARTIFACT_DIR="${ROOT_DIR}/artifacts/a30-battery"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [options]

Runs A30 CPU battery-current probes:
  1. 2 cores + performance/max CPU load for DURATION seconds
  2. 4 cores + performance/max CPU load for DURATION seconds

Options:
  --duration SECONDS        Load duration per condition. Default: ${DURATION}
  --sample-interval SEC     Battery sample interval. Default: ${SAMPLE_INTERVAL}
  --cooldown SECONDS        Idle cooldown between conditions. Default: ${COOLDOWN}
  --allow-external-power    Run even if AC/USB power is reported online.
  -h, --help                Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --duration)
      DURATION="${2:?missing duration}"
      shift
      ;;
    --sample-interval)
      SAMPLE_INTERVAL="${2:?missing sample interval}"
      shift
      ;;
    --cooldown)
      COOLDOWN="${2:?missing cooldown}"
      shift
      ;;
    --allow-external-power)
      ALLOW_EXTERNAL_POWER=1
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

case "$DURATION" in ''|*[!0-9]*) echo "error: --duration must be seconds" >&2; exit 2 ;; esac
case "$SAMPLE_INTERVAL" in ''|*[!0-9]*) echo "error: --sample-interval must be seconds" >&2; exit 2 ;; esac
case "$COOLDOWN" in ''|*[!0-9]*) echo "error: --cooldown must be seconds" >&2; exit 2 ;; esac

mkdir -p "$ARTIFACT_DIR"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
LOCAL_LOG="${ARTIFACT_DIR}/cpu-battery-${STAMP}.csv"

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "DURATION='$DURATION' SAMPLE_INTERVAL='$SAMPLE_INTERVAL' COOLDOWN='$COOLDOWN' ALLOW_EXTERNAL_POWER='$ALLOW_EXTERNAL_POWER' sh -s" <<'REMOTE' | tee "$LOCAL_LOG"
set -eu

B=/sys/class/power_supply/battery
AC=/sys/class/power_supply/ac
USB=/sys/class/power_supply/usb
CPU=/sys/devices/system/cpu
P=/sys/devices/system/cpu/cpu0/cpufreq
REMOTE_LOG=/mnt/SDCARD/plumos/logs/cpu-battery-last.csv
LOAD_PIDS=""

mkdir -p "$(dirname "$REMOTE_LOG")"
: > "$REMOTE_LOG"

line() {
  printf '%s\n' "$*"
  printf '%s\n' "$*" >> "$REMOTE_LOG"
}

read_file() {
  path="$1"
  if [ -r "$path" ]; then
    cat "$path" 2>/dev/null || printf '-'
  else
    printf '-'
  fi
}

power_mw() {
  voltage="$1"
  current="$2"
  awk -v v="$voltage" -v c="$current" 'BEGIN {
    if (v ~ /^[0-9]+$/ && c ~ /^[0-9]+$/) {
      printf "%.3f", (v * c) / 1000000000
    } else {
      printf "nan"
    }
  }'
}

sample() {
  sample_phase="$1"
  sample_start="$2"
  sample_cores="$3"
  now="$(date +%s)"
  elapsed=$((now - sample_start))
  governor="$(read_file "$P/scaling_governor")"
  online="$(read_file "$CPU/online")"
  min_freq="$(read_file "$P/scaling_min_freq")"
  max_freq="$(read_file "$P/scaling_max_freq")"
  cur_freq="$(read_file "$P/scaling_cur_freq")"
  capacity="$(read_file "$B/capacity")"
  voltage="$(read_file "$B/voltage_now")"
  current="$(read_file "$B/current_now")"
  status="$(read_file "$B/status")"
  ac_online="$(read_file "$AC/online")"
  usb_online="$(read_file "$USB/online")"
  temp="$(read_file "$B/temp")"
  loadavg="$(cut -d' ' -f1-3 /proc/loadavg 2>/dev/null | tr ' ' '|')"
  line "sample,$sample_phase,$now,$elapsed,$sample_cores,$governor,$online,$min_freq,$max_freq,$cur_freq,$capacity,$voltage,$current,$(power_mw "$voltage" "$current"),$status,$ac_online,$usb_online,$temp,$loadavg"
}

save_cpu() {
  CPUINFO_MIN="$(read_file "$P/cpuinfo_min_freq")"
  CPUINFO_MAX="$(read_file "$P/cpuinfo_max_freq")"
  case "$CPUINFO_MIN" in ''|*[!0-9]*) CPUINFO_MIN=120000 ;; esac
  case "$CPUINFO_MAX" in ''|*[!0-9]*) CPUINFO_MAX=1344000 ;; esac
  SAVED_GOV="$(read_file "$P/scaling_governor")"
  SAVED_MIN="$(read_file "$P/scaling_min_freq")"
  SAVED_MAX="$(read_file "$P/scaling_max_freq")"
  for c in 1 2 3; do
    eval "SAVED_CPU${c}=\$(read_file \"$CPU/cpu${c}/online\")"
  done
}

stop_load() {
  if [ -n "$LOAD_PIDS" ]; then
    kill $LOAD_PIDS 2>/dev/null || true
    sleep 1
    kill -KILL $LOAD_PIDS 2>/dev/null || true
    LOAD_PIDS=""
  fi
}

restore_cpu() {
  stop_load
  for c in 1 2 3; do
    [ -w "$CPU/cpu${c}/online" ] && echo 1 > "$CPU/cpu${c}/online" 2>/dev/null || true
  done
  [ -w "$P/scaling_min_freq" ] && echo "$CPUINFO_MIN" > "$P/scaling_min_freq" 2>/dev/null || true
  [ -w "$P/scaling_max_freq" ] && echo "$CPUINFO_MAX" > "$P/scaling_max_freq" 2>/dev/null || true
  case "$SAVED_MAX" in ''|*[!0-9]*) ;; *) echo "$SAVED_MAX" > "$P/scaling_max_freq" 2>/dev/null || true ;; esac
  case "$SAVED_MIN" in ''|*[!0-9]*) ;; *) echo "$SAVED_MIN" > "$P/scaling_min_freq" 2>/dev/null || true ;; esac
  [ -n "$SAVED_GOV" ] && [ "$SAVED_GOV" != "-" ] && echo "$SAVED_GOV" > "$P/scaling_governor" 2>/dev/null || true
  for c in 1 2 3; do
    eval "saved=\${SAVED_CPU${c}:-1}"
    case "$saved" in 0|1) [ -w "$CPU/cpu${c}/online" ] && echo "$saved" > "$CPU/cpu${c}/online" 2>/dev/null || true ;; esac
  done
}

set_perf_cores() {
  target_cores="$1"
  [ "$target_cores" = "2" ] || [ "$target_cores" = "4" ] || return 1
  [ -w "$CPU/cpu1/online" ] && echo 1 > "$CPU/cpu1/online" 2>/dev/null || true
  if [ "$target_cores" = "2" ]; then
    [ -w "$CPU/cpu2/online" ] && echo 0 > "$CPU/cpu2/online" 2>/dev/null || true
    [ -w "$CPU/cpu3/online" ] && echo 0 > "$CPU/cpu3/online" 2>/dev/null || true
  else
    [ -w "$CPU/cpu2/online" ] && echo 1 > "$CPU/cpu2/online" 2>/dev/null || true
    [ -w "$CPU/cpu3/online" ] && echo 1 > "$CPU/cpu3/online" 2>/dev/null || true
  fi
  echo "$CPUINFO_MAX" > "$P/scaling_max_freq" 2>/dev/null || true
  echo "$CPUINFO_MAX" > "$P/scaling_min_freq" 2>/dev/null || true
  echo performance > "$P/scaling_governor" 2>/dev/null || true
}

start_load() {
  load_count="$1"
  LOAD_PIDS=""
  i=0
  while [ "$i" -lt "$load_count" ]; do
    sh -c 'while :; do :; done' >/dev/null 2>&1 &
    LOAD_PIDS="$LOAD_PIDS $!"
    i=$((i + 1))
  done
}

run_condition() {
  condition_cores="$1"
  condition_phase="${condition_cores}core_perf"
  start_epoch="$(date +%s)"
  line "event,$condition_phase,$start_epoch,start,cores=$condition_cores"
  set_perf_cores "$condition_cores"
  sleep 1
  sample "${condition_phase}_pre" "$start_epoch" "$condition_cores"
  start_load "$condition_cores"
  load_start="$(date +%s)"
  while :; do
    now="$(date +%s)"
    elapsed=$((now - load_start))
    if [ "$elapsed" -gt "$DURATION" ]; then
      break
    fi
    sample "$condition_phase" "$load_start" "$condition_cores"
    sleep "$SAMPLE_INTERVAL"
  done
  stop_load
  sample "${condition_phase}_post" "$load_start" "$condition_cores"
  line "event,$condition_phase,$(date +%s),end,cores=$condition_cores"
}

save_cpu
trap restore_cpu EXIT INT TERM

line "record,phase,t,elapsed_s,cores,governor,online,min_freq,max_freq,cur_freq,capacity,voltage_now,current_now,power_mw,status,ac_online,usb_online,temp,loadavg"
line "meta,duration,$DURATION"
line "meta,sample_interval,$SAMPLE_INTERVAL"
line "meta,cooldown,$COOLDOWN"
line "meta,cpuinfo_min,$CPUINFO_MIN"
line "meta,cpuinfo_max,$CPUINFO_MAX"
line "meta,saved_governor,$SAVED_GOV"
line "meta,saved_min,$SAVED_MIN"
line "meta,saved_max,$SAVED_MAX"

ac_now="$(read_file "$AC/online")"
usb_now="$(read_file "$USB/online")"
if [ "$ALLOW_EXTERNAL_POWER" != "1" ] && { [ "$ac_now" != "0" ] || [ "$usb_now" != "0" ]; }; then
  line "error,external_power_online,ac=$ac_now,usb=$usb_now"
  exit 3
fi

base_start="$(date +%s)"
sample idle_before "$base_start" 4
run_condition 2
restore_cpu
cool_start="$(date +%s)"
while [ $(( $(date +%s) - cool_start )) -lt "$COOLDOWN" ]; do
  sample cooldown_after_2core "$cool_start" 4
  sleep "$SAMPLE_INTERVAL"
done
run_condition 4
restore_cpu
sample restored "$(date +%s)" 4
line "event,probe,$(date +%s),complete,remote_log=$REMOTE_LOG"
REMOTE

echo "Local log: $LOCAL_LOG"
awk -F, '
  $1 == "sample" && ($2 == "2core_perf" || $2 == "4core_perf") {
    phase=$2
    n[phase]++
    current[phase] += $13
    voltage[phase] += $12
    power[phase] += $14
    cap_first[phase] = (phase in cap_first) ? cap_first[phase] : $11
    cap_last[phase] = $11
  }
  END {
    for (i = 1; i <= 2; i++) {
      phase = (i == 1) ? "2core_perf" : "4core_perf"
      if (n[phase] > 0) {
        printf "summary,%s,samples=%d,avg_current_now=%.0f,avg_voltage_now=%.0f,avg_power_mw=%.3f,capacity=%s->%s\n",
          phase, n[phase], current[phase]/n[phase], voltage[phase]/n[phase],
          power[phase]/n[phase], cap_first[phase], cap_last[phase]
      }
    }
    if (n["2core_perf"] > 0 && n["4core_perf"] > 0) {
      printf "summary,ratio,avg_current_4core_over_2core=%.3f,avg_power_4core_over_2core=%.3f\n",
        (current["4core_perf"]/n["4core_perf"]) / (current["2core_perf"]/n["2core_perf"]),
        (power["4core_perf"]/n["4core_perf"]) / (power["2core_perf"]/n["2core_perf"])
    }
  }
' "$LOCAL_LOG"
