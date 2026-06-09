#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET="${A30_TARGET:-root@192.168.10.165}"
PORT="${A30_SSH_PORT:-2222}"
OUT_DIR="${1:-${ROOT_DIR}/artifacts/a30-captures}"
SSH_OPTS=(-p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR)

usage() {
  cat <<EOF
Usage: A30_TARGET=root@A30_IP $0 [OUT_DIR]

Captures /dev/fb0 from the A30, crops the visible framebuffer page, and writes:
  *.probe.txt
  *.fb0.raw
  *.visible.png
  *.visible.cw.png
  *.visible.ccw.png

Defaults:
  OUT_DIR       ${ROOT_DIR}/artifacts/a30-captures
  A30_TARGET    ${TARGET}
  A30_SSH_PORT  ${PORT}
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "error: ImageMagick 'magick' is required for PNG conversion" >&2
  exit 127
fi

mkdir -p "$OUT_DIR"
stamp="$(date +%Y%m%d-%H%M%S)"
probe_path="${OUT_DIR}/${stamp}.probe.txt"
raw_path="${OUT_DIR}/${stamp}.fb0.raw"
png_path="${OUT_DIR}/${stamp}.visible.png"

probe="$(
  ssh "${SSH_OPTS[@]}" "$TARGET" \
    '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --no-input --no-audio --draw-ms 0 2>/dev/null'
)"
printf '%s\n' "$probe" >"$probe_path"

video="$(printf '%s\n' "$probe" | sed -n 's/^video //p' | head -n 1)"
xres="$(printf '%s\n' "$video" | sed -n 's/.*xres=\([0-9][0-9]*\).*/\1/p')"
yres="$(printf '%s\n' "$video" | sed -n 's/.*yres=\([0-9][0-9]*\).*/\1/p')"
virt="$(printf '%s\n' "$video" | sed -n 's/.*virtual=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p')"
offset="$(printf '%s\n' "$video" | sed -n 's/.*offset=\([0-9][0-9]*,[0-9][0-9]*\).*/\1/p')"
bpp="$(printf '%s\n' "$video" | sed -n 's/.*bpp=\([0-9][0-9]*\).*/\1/p')"
line="$(printf '%s\n' "$video" | sed -n 's/.*line=\([0-9][0-9]*\).*/\1/p')"

if [ -z "$xres" ] || [ -z "$yres" ] || [ -z "$virt" ] || [ -z "$offset" ]; then
  echo "error: could not parse framebuffer geometry from probe output" >&2
  cat "$probe_path" >&2
  exit 1
fi

vx="${virt%x*}"
vy="${virt#*x}"
ox="${offset%,*}"
oy="${offset#*,}"
if [ -z "$line" ]; then
  line=$((vx * bpp / 8))
fi

ssh "${SSH_OPTS[@]}" "$TARGET" \
  "dd if=/dev/fb0 bs=${line} count=${vy} 2>/dev/null" >"$raw_path"

case "${bpp:-32}" in
  32)
    magick -size "${vx}x${vy}" -depth 8 BGRA:"$raw_path" \
      -crop "${xres}x${yres}+${ox}+${oy}" +repage -alpha off "$png_path"
    ;;
  16)
    fb16_format=${A30_FB_16_FORMAT:-RGB565}
    magick -size "${vx}x${vy}" "${fb16_format}:$raw_path" \
      -crop "${xres}x${yres}+${ox}+${oy}" +repage "$png_path"
    ;;
  *)
    echo "error: unsupported framebuffer bpp=${bpp}; expected 16 or 32" >&2
    exit 1
    ;;
esac
magick "$png_path" -rotate 90 "${png_path%.png}.cw.png"
magick "$png_path" -rotate -90 "${png_path%.png}.ccw.png"

identify "$png_path"
printf 'probe=%s\nraw=%s\npng=%s\ncw=%s\nccw=%s\n' \
  "$probe_path" \
  "$raw_path" \
  "$png_path" \
  "${png_path%.png}.cw.png" \
  "${png_path%.png}.ccw.png"
