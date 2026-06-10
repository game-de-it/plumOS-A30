#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SYSTEMS_JSON="${ROOT_DIR}/package/frontend/plumos/config/frontend/systems.json"
SOURCES_TSV="${ROOT_DIR}/package/frontend/plumos/config/frontend/scraper-sources.tsv"
OUTPUT_DIR="${ROOT_DIR}/build/thumbnail-scraper-cache"
KINDS="Named_Boxarts Named_Snaps Named_Titles"
SYSTEM_FILTER=""
FORCE=0

usage() {
  cat <<EOF
Usage: $0 [options]

Prefetch libretro DAT CRC indexes and thumbnail directory indexes for the
plumOS thumbnail scraper. Generated data is intended for dist/package output,
not for git.

Options:
  --systems-json PATH  systems.json to read enabled scraper systems from.
                       Default: ${SYSTEMS_JSON}
  --sources PATH       scraper-sources.tsv.
                       Default: ${SOURCES_TSV}
  --output PATH        Output cache root.
                       Default: ${OUTPUT_DIR}
  --system ID          Prefetch one system only.
  --kind KIND          Thumbnail kind to prefetch. Can be repeated.
                       Default: ${KINDS}
  --force              Re-download source DAT/HTML files.
  -h, --help           Show this help.
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

is_enabled_system() {
  local system="$1"
  python3 - "$SYSTEMS_JSON" "$system" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

target = sys.argv[2]
for system in data.get("systems", []):
    if system.get("id") != target:
        continue
    scraper = system.get("scraper") or {}
    sys.exit(0 if scraper.get("enabled") is True else 1)
sys.exit(1)
PY
}

quote_url_component() {
  python3 - "$1" "$2" <<'PY'
import sys
from urllib.parse import quote

print(quote(sys.argv[1], safe=sys.argv[2]))
PY
}

dat_to_crc_index() {
  python3 - "$1" "$2" <<'PY'
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
game = ""
with open(src, encoding="utf-8", errors="replace") as handle, open(dst, "w", encoding="utf-8") as out:
    for raw in handle:
        line = raw.strip()
        if line.startswith("game"):
            game = ""
            continue
        if not game:
            match = re.match(r'name\s+"(.*)"$', line)
            if match:
                game = match.group(1)
            continue
        match = re.search(r'\bcrc\s+([0-9A-Fa-f]{8})\b', line)
        if match:
            out.write(f"{match.group(1).lower()}\t{game}\n")
PY
}

html_to_thumb_index() {
  python3 - "$1" "$2" "$3" <<'PY'
import html
import re
import sys

src, tsv_dst, names_dst = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(src, encoding="utf-8", errors="replace").read()
rows = []

for href, label in re.findall(r'<a\s+[^>]*href="([^"]+\.png)"[^>]*>([^<]+\.png)</a>', text, re.I):
    if href.startswith("/"):
        continue
    label = html.unescape(label)
    base = label[:-4] if label.lower().endswith(".png") else label
    rows.append((base, href))

rows.sort(key=lambda item: item[0].casefold())
with open(tsv_dst, "w", encoding="utf-8") as tsv, open(names_dst, "w", encoding="utf-8") as names:
    for base, href in rows:
        tsv.write(f"{base}\t{href}\n")
        names.write(f"{base}.png\n")
PY
}

fetch_url() {
  local url="$1"
  local output="$2"

  mkdir -p "$(dirname "$output")"
  if [ "$FORCE" -eq 0 ] && [ -s "$output" ]; then
    return 0
  fi

  tmp="${output}.tmp.$$"
  rm -f "$tmp"
  curl -fsSL --retry 2 --connect-timeout 15 --max-time 120 -o "$tmp" "$url"
  mv "$tmp" "$output"
}

prefetch_one() {
  local system="$1"
  local playlist="$2"
  local dat_path="$3"
  local dat_url dat_file crc_index encoded_playlist kind html_url html_file tsv_file names_file count

  if [ -n "$SYSTEM_FILTER" ] && [ "$SYSTEM_FILTER" != "$system" ]; then
    return 0
  fi
  if ! is_enabled_system "$system"; then
    return 0
  fi

  dat_url="https://raw.githubusercontent.com/libretro/libretro-database/master/$(quote_url_component "$dat_path" "/")"
  dat_file="${OUTPUT_DIR}/dat-source/${system}.dat"
  crc_index="${OUTPUT_DIR}/dat/${system}.crc.tsv"

  printf 'fetch_dat\t%s\t%s\n' "$system" "$dat_url"
  mkdir -p "$(dirname "$crc_index")"
  fetch_url "$dat_url" "$dat_file"
  dat_to_crc_index "$dat_file" "${crc_index}.tmp"
  mv "${crc_index}.tmp" "$crc_index"
  count="$(wc -l < "$crc_index" | tr -d ' ')"
  printf 'index_dat\t%s\trows=%s\t%s\n' "$system" "$count" "$crc_index"

  encoded_playlist="$(quote_url_component "$playlist" "")"
  for kind in $KINDS; do
    html_url="http://thumbnails.libretro.com/${encoded_playlist}/${kind}/"
    html_file="${OUTPUT_DIR}/thumb-html/${system}/${kind}.html"
    tsv_file="${OUTPUT_DIR}/thumb-names/${system}/${kind}.tsv"
    names_file="${OUTPUT_DIR}/thumb-names/${system}/${kind}.names"

    printf 'fetch_thumb_index\t%s\t%s\t%s\n' "$system" "$kind" "$html_url"
    mkdir -p "$(dirname "$tsv_file")"
    fetch_url "$html_url" "$html_file"
    html_to_thumb_index "$html_file" "${tsv_file}.tmp" "${names_file}.tmp"
    mv "${tsv_file}.tmp" "$tsv_file"
    mv "${names_file}.tmp" "$names_file"
    count="$(wc -l < "$tsv_file" | tr -d ' ')"
    printf 'index_thumb\t%s\t%s\trows=%s\t%s\n' "$system" "$kind" "$count" "$tsv_file"
  done
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --systems-json)
      SYSTEMS_JSON="${2:-}"
      shift 2
      ;;
    --sources)
      SOURCES_TSV="${2:-}"
      shift 2
      ;;
    --output)
      OUTPUT_DIR="${2:-}"
      shift 2
      ;;
    --system)
      SYSTEM_FILTER="${2:-}"
      shift 2
      ;;
    --kind)
      KINDS="${2:-}"
      shift 2
      ;;
    --force)
      FORCE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
done

[ -r "$SYSTEMS_JSON" ] || die "systems.json is not readable: $SYSTEMS_JSON"
[ -r "$SOURCES_TSV" ] || die "sources TSV is not readable: $SOURCES_TSV"
[ -n "$OUTPUT_DIR" ] || die "--output is required"

need_cmd curl
need_cmd python3
need_cmd wc

mkdir -p "$OUTPUT_DIR"
install -m 0644 "$SOURCES_TSV" "${OUTPUT_DIR}/sources.tsv"

{
  echo "thumbnail scraper cache"
  echo "generated_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "systems_json=$(basename "$SYSTEMS_JSON")"
  echo "sources=$(basename "$SOURCES_TSV")"
  echo "kinds=${KINDS}"
} > "${OUTPUT_DIR}/manifest.txt"

while IFS=$'\t' read -r system playlist dat_path; do
  case "$system" in
    ""|\#*) continue ;;
  esac
  [ -n "$playlist" ] || die "missing playlist for $system"
  [ -n "$dat_path" ] || die "missing DAT path for $system"
  prefetch_one "$system" "$playlist" "$dat_path"
done < "$SOURCES_TSV"

find "$OUTPUT_DIR" -type f | sort | xargs shasum -a 256 > "${OUTPUT_DIR}/SHA256SUMS"
