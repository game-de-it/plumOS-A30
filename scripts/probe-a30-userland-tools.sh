#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY=0
ARTIFACT_DIR=""

usage() {
  cat <<EOF
Usage: A30_TARGET=root@IP $0 [options]

Checks the plumOS BusyBox/userland tools on an A30 using only the plumOS
runtime PATH inside the test shell.

Options:
  --deploy              Deploy dist/plumos-userland before probing.
  --artifact-dir DIR    Write logs under DIR.
  -h, --help            Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      ;;
    --artifact-dir)
      ARTIFACT_DIR="${2:?missing artifact directory}"
      shift
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

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "$ROOT_DIR/scripts/deploy-a30.sh" \
    "$ROOT_DIR/dist/plumos-userland" /mnt/SDCARD
fi

if [ -z "$ARTIFACT_DIR" ]; then
  ARTIFACT_DIR="$ROOT_DIR/artifacts/a30-probes/userland-tools/$(date +%Y%m%d-%H%M%S)"
fi
mkdir -p "$ARTIFACT_DIR"
REMOTE_LOG="$ARTIFACT_DIR/remote.log"
SUMMARY="$ARTIFACT_DIR/summary.md"

remote_script=""
read -r -d '' remote_script <<'REMOTE' || true
set -u

PLUMOS_ROOT=/mnt/SDCARD/plumos
BUSYBOX="$PLUMOS_ROOT/bin/busybox"
PATH_UNDER_TEST="$PLUMOS_ROOT/gnu/bin:$PLUMOS_ROOT/bin"

PASS_COUNT=0
LIMIT_COUNT=0
MISSING_COUNT=0
FAIL_COUNT=0

run_in_plumos() {
  "$BUSYBOX" env -i \
    PLUMOS_ROOT="$PLUMOS_ROOT" \
    PATH="$PATH_UNDER_TEST" \
    HOME=/tmp \
    TERM=dumb \
    "$BUSYBOX" sh -c "$1"
}

record_tool() {
  name="$1"
  status="$2"
  path="$3"
  rc="$4"
  note="$5"

  case "$status" in
    pass) PASS_COUNT=$((PASS_COUNT + 1)) ;;
    usable_with_limits) LIMIT_COUNT=$((LIMIT_COUNT + 1)) ;;
    missing) MISSING_COUNT=$((MISSING_COUNT + 1)) ;;
    fail) FAIL_COUNT=$((FAIL_COUNT + 1)) ;;
  esac

  printf 'tool_status name=%s status=%s path=%s rc=%s note=%s\n' \
    "$name" "$status" "${path:-none}" "$rc" "$note"
}

check_tool() {
  name="$1"
  ok_status="$2"
  note="$3"
  cmd="$4"

  printf '\n## %s\n' "$name"
  path="$(run_in_plumos "command -v $name" 2>/dev/null | "$BUSYBOX" head -n 1 || true)"
  if [ -z "$path" ]; then
    echo "missing: $name is not present in $PATH_UNDER_TEST"
    record_tool "$name" missing "" 127 not_in_plumos_path
    return 0
  fi
  echo "path=$path"

  out="/tmp/plumos-userland-${name}-$$.out"
  run_in_plumos "$cmd" >"$out" 2>&1
  rc=$?
  "$BUSYBOX" sed -n '1,40p' "$out"
  lines="$("$BUSYBOX" wc -l <"$out" | "$BUSYBOX" tr -d ' ')"
  if [ "${lines:-0}" -gt 40 ]; then
    echo "... truncated ${lines} lines ..."
  fi
  "$BUSYBOX" rm -f "$out"

  if [ "$rc" -eq 0 ]; then
    record_tool "$name" "$ok_status" "$path" "$rc" "$note"
  else
    record_tool "$name" fail "$path" "$rc" basic_probe_failed
  fi
}

echo "plumos_userland_probe_start"
echo "date_utc=$("$BUSYBOX" date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date)"
echo "kernel=$("$BUSYBOX" uname -a 2>/dev/null || uname -a)"
echo "plumos_root=$PLUMOS_ROOT"
echo "path_under_test=$PATH_UNDER_TEST"

if [ ! -x "$BUSYBOX" ]; then
  echo "fatal: $BUSYBOX is not executable"
  echo "result=userland_tools_busybox_missing"
  exit 1
fi

echo "busybox_version=$("$BUSYBOX" | "$BUSYBOX" head -n 1)"

check_tool busybox pass busybox_static_binary \
  'busybox --help | head -n 1'
check_tool plumos-env pass clean_path_entrypoint \
  'plumos-env sh -c "echo env_ok; echo PATH=$PATH; command -v ps; command -v busybox"'
check_tool ps usable_with_limits busybox_ps_wide_available \
  'ps w | head -n 8'
check_tool top usable_with_limits busybox_top_batch_available \
  'top -b -n 1 | head -n 8'
check_tool df pass busybox_df_basic \
  'df -h "$PLUMOS_ROOT"'
check_tool free pass busybox_free_basic \
  'free'
check_tool tar pass busybox_tar_create_list \
  'tmp=/tmp/plumos-userland-tar-$$; rm -rf "$tmp"; mkdir -p "$tmp/in"; printf "%s\n" ok > "$tmp/in/file.txt"; tar -cf "$tmp/test.tar" -C "$tmp/in" file.txt; tar -tf "$tmp/test.tar" | grep -qx file.txt; rm -rf "$tmp"'
check_tool find pass busybox_find_maxdepth \
  'find "$PLUMOS_ROOT/bin" -maxdepth 1 -name busybox -type f -print | grep -q busybox'
check_tool grep pass busybox_grep_pipe \
  'printf "%s\n" alpha | grep -qx alpha'
check_tool sed pass busybox_sed_substitute \
  'printf "%s\n" alpha | sed "s/alpha/beta/" | grep -qx beta'
check_tool awk pass busybox_awk_field \
  'printf "%s\n" "alpha beta" | awk "{print \$2}" | grep -qx beta'
check_tool ip usable_with_limits ip_link_basic \
  'ip link show lo'
check_tool ss usable_with_limits socket_listing_available \
  'ss -lun | head -n 20'
check_tool netstat usable_with_limits busybox_netstat_socket_listing \
  'netstat -anu | head -n 20'
check_tool lsof usable_with_limits busybox_lsof_listing \
  'lsof | head -n 20'
check_tool strace pass strace_exec_trace \
  'tmp=/tmp/plumos-userland-strace-$$.log; rm -f "$tmp"; strace -o "$tmp" busybox true; rc=$?; tail -n 5 "$tmp"; rm -f "$tmp"; exit "$rc"'
check_tool timeout pass busybox_timeout_expected_exit \
  'timeout 1 sh -c "sleep 2"; rc=$?; echo timeout_rc=$rc; case "$rc" in 124|137|143) exit 0 ;; *) exit "$rc" ;; esac'

printf '\nsummary pass=%s usable_with_limits=%s missing=%s fail=%s\n' \
  "$PASS_COUNT" "$LIMIT_COUNT" "$MISSING_COUNT" "$FAIL_COUNT"

if [ "$FAIL_COUNT" -eq 0 ] && [ "$MISSING_COUNT" -eq 0 ]; then
  echo "result=userland_tools_probe_ok"
elif [ "$FAIL_COUNT" -eq 0 ]; then
  echo "result=userland_tools_probe_has_missing_tools"
else
  echo "result=userland_tools_probe_failed"
fi
REMOTE

A30_TARGET="$TARGET" "$ROOT_DIR/scripts/run-a30.sh" "$remote_script" | tee "$REMOTE_LOG"

result="$(grep '^result=' "$REMOTE_LOG" | tail -n 1 | cut -d= -f2- || true)"
result="${result:-unknown}"

{
  echo "# A30 userland tools probe"
  echo
  echo "- target: $TARGET"
  echo "- artifact: $ARTIFACT_DIR"
  echo "- result: $result"
  echo
  echo "## Tool status"
  echo
  awk '
    /^tool_status / {
      name=""; status=""; path=""; rc=""; note="";
      for (i = 2; i <= NF; i++) {
        split($i, kv, "=");
        if (kv[1] == "name") name=kv[2];
        if (kv[1] == "status") status=kv[2];
        if (kv[1] == "path") path=kv[2];
        if (kv[1] == "rc") rc=kv[2];
        if (kv[1] == "note") note=kv[2];
      }
      printf "- `%s`: %s (rc=%s, path=%s, note=%s)\n", name, status, rc, path, note;
    }
  ' "$REMOTE_LOG"
  echo
  echo "## Summary"
  echo
  grep '^summary ' "$REMOTE_LOG" || true
} > "$SUMMARY"

echo "summary=$SUMMARY"
