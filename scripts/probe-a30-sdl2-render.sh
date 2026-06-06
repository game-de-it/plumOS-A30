#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
DEPLOY="${PLUMOS_DEPLOY_SDL2_PROBE:-0}"
RUN_MS="${PLUMOS_SDL2_RENDER_PROBE_MS:-0}"
CASES="${PLUMOS_SDL2_RENDER_CASES:-auto dummy offscreen evdev kmsdrm}"

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [--deploy] [--run-ms MS] [--cases "auto dummy offscreen evdev kmsdrm"]

Checks whether the bundled SDL2 API runtime has a real video/render backend on
the A30. The "auto" case runs without SDL_VIDEODRIVER; other cases force that
driver name.

Environment:
  PLUMOS_DEPLOY_SDL2_PROBE    Deploy dist/plumos-sdl2-probe first. Default: ${DEPLOY}
  PLUMOS_SDL2_RENDER_PROBE_MS Probe delay after rendering. Default: ${RUN_MS}
  PLUMOS_SDL2_RENDER_CASES    Space-separated driver cases. Default: ${CASES}
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --deploy)
      DEPLOY=1
      shift
      ;;
    --run-ms)
      RUN_MS="${2:?missing --run-ms value}"
      shift 2
      ;;
    --cases)
      CASES="${2:?missing --cases value}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

case "$DEPLOY" in
  0|1) ;;
  *)
    echo "error: PLUMOS_DEPLOY_SDL2_PROBE must be 0 or 1: $DEPLOY" >&2
    exit 2
    ;;
esac

case "$RUN_MS" in
  ''|*[!0-9]*)
    echo "error: --run-ms must be a non-negative integer: $RUN_MS" >&2
    exit 2
    ;;
esac

if [ "$DEPLOY" = "1" ]; then
  A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/deploy-a30.sh" \
    "${ROOT_DIR}/dist/plumos-sdl2-probe" /mnt/SDCARD
fi

A30_TARGET="$TARGET" "${ROOT_DIR}/scripts/run-a30.sh" \
  "PLUMOS_SDL2_RENDER_CASES='$CASES'
PLUMOS_SDL2_RENDER_PROBE_MS='$RUN_MS'
set -eu

PROBE=/mnt/SDCARD/plumos/bin/plumos-sdl2-probe
TMPDIR=/tmp/plumos-sdl2-render
rm -rf \"\$TMPDIR\"
mkdir -p \"\$TMPDIR\"

echo '== device inventory =='
ls -l /dev/fb* /dev/dri/* /dev/mali /dev/disp /dev/ion 2>/dev/null || true
echo '== proc fb =='
cat /proc/fb 2>/dev/null || true
echo '== modules =='
cat /proc/modules 2>/dev/null | grep -Ei 'drm|mali|ump|disp|fb|sunxi' || true
echo '== runtime libs =='
ls -l /mnt/SDCARD/plumos/lib/libSDL2-2.0.so.0 /mnt/SDCARD/plumos/lib/libSDL3.so.0 2>/dev/null || true

real_ok=0
any_render_ok=0

for driver in \$PLUMOS_SDL2_RENDER_CASES; do
  log=\"\$TMPDIR/\$driver.log\"
  echo \"== case: \$driver ==\"
  set +e
  if [ \"\$driver\" = auto ]; then
    unset SDL_VIDEODRIVER
    PLUMOS_SDL2_DEFAULT_DUMMY=0 \
      SDL_AUDIODRIVER=dummy \
      PLUMOS_ROOT=/mnt/SDCARD/plumos \
      \"\$PROBE\" --graphics-only --render-test --window-visible \
        --timeout-ms \"\$PLUMOS_SDL2_RENDER_PROBE_MS\" >\"\$log\" 2>&1
  else
    SDL_VIDEODRIVER=\"\$driver\" \
      PLUMOS_SDL2_DEFAULT_DUMMY=0 \
      SDL_AUDIODRIVER=dummy \
      PLUMOS_ROOT=/mnt/SDCARD/plumos \
      \"\$PROBE\" --graphics-only --render-test --window-visible \
        --timeout-ms \"\$PLUMOS_SDL2_RENDER_PROBE_MS\" >\"\$log\" 2>&1
  fi
  rc=\$?
  set -e
  cat \"\$log\"
  echo \"case_rc=\$rc\"

  current_driver=\$(sed -n 's/.*current_video_driver=\\([^ ]*\\).*/\\1/p' \"\$log\" | tail -n 1)
  if [ \"\$rc\" -eq 0 ] && grep -q 'render create=.*yes' \"\$log\"; then
    any_render_ok=1
    case \"\$current_driver\" in
      ''|-|dummy|offscreen|evdev) ;;
      *) real_ok=1 ;;
    esac
  fi
done

echo '== result =='
if [ \"\$real_ok\" -eq 1 ]; then
  echo result=sdl2_real_render_backend_visible
elif [ \"\$any_render_ok\" -eq 1 ]; then
  echo result=sdl2_renderer_only_dummy_offscreen_or_evdev
else
  echo result=sdl2_render_backend_missing
fi

rm -rf \"\$TMPDIR\""
