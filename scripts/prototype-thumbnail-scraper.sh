#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CACHE_ROOT="${PLUMOS_THUMB_CACHE:-"${ROOT_DIR}/artifacts/thumb-scraper/cache"}"
REPORT_PATH=""
CANDIDATE_REPORT_PATH=""
MODE="dry-run"
KIND="Named_Boxarts"
SYSTEM_FILTER="all"
LIMIT=0
RETRY_NEGATIVE=0
LOOSE_INDEX=0
CANDIDATE_LIMIT=5
CANDIDATE_MIN_SCORE=60

if [ -n "${PLUMOS_IMAGE_ROOT:-}" ]; then
  IMAGE_ROOT="$PLUMOS_IMAGE_ROOT"
elif [ -n "${PLUMOS_SDCARD_ROOT:-}" ]; then
  IMAGE_ROOT="${PLUMOS_SDCARD_ROOT%/}/Images"
else
  IMAGE_ROOT="${ROOT_DIR}/artifacts/thumb-scraper/Images"
fi

NES_ROM_DIR="${ROOT_DIR}/artifacts/reference/nes"
GB_ROM_DIR="${ROOT_DIR}/artifacts/reference/gb"

usage() {
  cat <<'EOF'
Usage: scripts/prototype-thumbnail-scraper.sh [options]

Shell prototype for resolving/downloading ROM thumbnails before FE integration.
Default mode is dry-run: it computes matches and URLs but does not write images.

Options:
  --fetch              Download thumbnails. Without this, dry-run only.
  --dry-run            Compute only. This is the default.
  --system ID          all, nes, fds, or gb. Default: all.
  --kind KIND          Named_Boxarts, Named_Snaps, or Named_Titles.
                      Default: Named_Boxarts.
  --nes-dir PATH       NES/FDS reference ROM directory.
                      Default: artifacts/reference/nes.
  --gb-dir PATH        GB reference ROM directory.
                      Default: artifacts/reference/gb.
  --image-root PATH    Destination image root. Final A30 root is /mnt/SDCARD/Images.
  --cache-root PATH    Download/index cache root.
  --report PATH        Write tab-separated report.
  --candidate-report PATH
                      Write ranked thumbnail suggestions for no_match ROMs.
                      Suggestions are advisory and are never auto-downloaded.
  --candidate-limit N  Number of suggestions per no_match ROM.
                      Default: 5.
  --candidate-min-score N
                      Minimum suggestion score from 0 to 100.
                      Default: 60.
  --limit N            Stop after N candidate ROMs.
  --retry-negative     Ignore previous negative cache entries.
  --loose-index        Fetch the large thumbnail directory index and try
                      normalized title matching when exact URLs fail.
  -h, --help           Show this help.

Output columns:
  status system method crc rom_path dest_path thumbnail_name url
  For exists and negative_cached, crc can be "-" because those checks happen
  before reading the ROM payload.

Statuses:
  exists, would_download, downloaded, no_match, negative_cached,
  download_failed, invalid_png, skip_zip, skip_ext
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

urlencode_component() {
  python3 - "$1" <<'PY'
import sys
from urllib.parse import quote
print(quote(sys.argv[1], safe=""))
PY
}

normalize_key() {
  python3 - "$1" <<'PY'
import re
import sys
import unicodedata

s = sys.argv[1]
s = unicodedata.normalize("NFKC", s)
s = re.sub(r"\.[A-Za-z0-9]{2,4}$", "", s)
s = re.sub(r"\[[^\]]*\]", "", s)
s = re.sub(r"\([^)]*\)", "", s)
s = s.casefold()
print("".join(ch for ch in s if ch.isalnum()))
PY
}

html_index_to_names() {
  python3 - "$1" <<'PY'
import html
import re
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    text = handle.read()

for href, label in re.findall(r'<a\s+[^>]*href="([^"]+\.png)"[^>]*>([^<]+\.png)</a>', text, re.I):
    print(html.unescape(label))
PY
}

safe_cache_name() {
  printf '%s' "$1" | tr '/ %' '___'
}

system_dat_url() {
  case "$1" in
    nes) printf '%s\n' 'https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/no-intro/Nintendo%20-%20Nintendo%20Entertainment%20System.dat' ;;
    fds) printf '%s\n' 'https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/no-intro/Nintendo%20-%20Family%20Computer%20Disk%20System.dat' ;;
    gb) printf '%s\n' 'https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/no-intro/Nintendo%20-%20Game%20Boy.dat' ;;
    *) return 1 ;;
  esac
}

system_thumb_playlist() {
  case "$1" in
    nes) printf '%s\n' 'Nintendo - Nintendo Entertainment System' ;;
    fds) printf '%s\n' 'Nintendo - Family Computer Disk System' ;;
    gb) printf '%s\n' 'Nintendo - Game Boy' ;;
    *) return 1 ;;
  esac
}

system_rom_root() {
  case "$1" in
    nes|fds) printf '%s\n' "$NES_ROM_DIR" ;;
    gb) printf '%s\n' "$GB_ROM_DIR" ;;
    *) return 1 ;;
  esac
}

system_for_ext() {
  case "$1" in
    nes) printf 'nes\n' ;;
    fds) printf 'fds\n' ;;
    gb) printf 'gb\n' ;;
    *) return 1 ;;
  esac
}

lower_ext() {
  local name="$1"
  name="${name##*/}"
  if [[ "$name" != *.* ]]; then
    return 1
  fi
  printf '%s\n' "${name##*.}" | tr '[:upper:]' '[:lower:]'
}

strip_ext() {
  local name="$1"
  name="${name%.*}"
  printf '%s\n' "$name"
}

existing_thumbnail_path() {
  local system="$1"
  local output_rel="$2"
  local rom_stem="$3"
  python3 - "${IMAGE_ROOT%/}" "$system" "$output_rel" "$rom_stem" <<'PY'
import os
import sys

image_root, system, output_rel, rom_stem = sys.argv[1:5]
root = os.path.join(image_root, system)
extensions = ("png", "jpg", "jpeg", "webp")
seen = set()

for stem in (output_rel, rom_stem):
    stem = stem.strip("/")
    if not stem:
        continue
    rel_dir, base = os.path.split(stem)
    directory = os.path.join(root, rel_dir)
    key = (os.path.normcase(directory), os.path.normcase(base))
    if key in seen:
        continue
    seen.add(key)
    try:
        entries = os.listdir(directory)
    except OSError:
        continue
    by_name = {entry.casefold(): entry for entry in entries}
    for ext in extensions:
        entry = by_name.get(f"{base}.{ext}".casefold())
        if not entry:
            continue
        path = os.path.join(directory, entry)
        if os.path.isfile(path) and os.path.getsize(path) > 0:
            print(path)
            sys.exit(0)

sys.exit(1)
PY
}

file_identity() {
  python3 - "$1" <<'PY'
import os
import sys

st = os.stat(sys.argv[1])
print(f"{st.st_size}\t{int(st.st_mtime)}")
PY
}

relative_to_root() {
  local root="${1%/}"
  local path="$2"
  if [[ "$path" == "$root/"* ]]; then
    printf '%s\n' "${path#"$root/"}"
  else
    printf '%s\n' "$(basename "$path")"
  fi
}

ensure_dat_index() {
  local system="$1"
  local dir="${CACHE_ROOT}/dat"
  local dat="${dir}/${system}.dat"
  local index="${dir}/${system}.crc.tsv"
  local url

  mkdir -p "$dir"
  if [ ! -s "$dat" ]; then
    url="$(system_dat_url "$system")"
    printf 'fetch_dat\t%s\t%s\n' "$system" "$url" >&2
    curl -fsSL --retry 2 -o "${dat}.tmp" "$url"
    mv "${dat}.tmp" "$dat"
  fi
  if [ ! -s "$index" ] || [ "$dat" -nt "$index" ]; then
    awk '
      /^game[[:space:]]*\(/ { game=""; next }
      /^[[:space:]]*name[[:space:]]+"/ && game == "" {
        line=$0
        sub(/^[[:space:]]*name[[:space:]]+"/, "", line)
        sub(/"$/, "", line)
        game=line
        next
      }
      /^[[:space:]]*rom[[:space:]]*\(/ {
        if (match($0, /crc[[:space:]][0-9A-Fa-f]+/)) {
          crc=substr($0, RSTART + 4, RLENGTH - 4)
          gsub(/[[:space:]]/, "", crc)
          print tolower(crc) "\t" game
        }
      }
    ' "$dat" >"${index}.tmp"
    mv "${index}.tmp" "$index"
  fi
  printf '%s\n' "$index"
}

ensure_thumb_names() {
  local system="$1"
  local playlist encoded dir html names base
  playlist="$(system_thumb_playlist "$system")"
  encoded="$(urlencode_component "$playlist")"
  dir="${CACHE_ROOT}/thumb-index/${system}"
  html="${dir}/${KIND}.html"
  names="${dir}/${KIND}.names"
  base="https://thumbnails.libretro.com/${encoded}/${KIND}/"

  mkdir -p "$dir"
  if [ ! -s "$html" ]; then
    printf 'fetch_thumb_index\t%s\t%s\n' "$system" "$base" >&2
    curl -fsSL --retry 2 -o "${html}.tmp" "$base"
    mv "${html}.tmp" "$html"
  fi
  if [ ! -s "$names" ] || [ "$html" -nt "$names" ]; then
    html_index_to_names "$html" >"${names}.tmp"
    mv "${names}.tmp" "$names"
  fi
  printf '%s\n' "$names"
}

thumb_base_url() {
  local playlist encoded
  playlist="$(system_thumb_playlist "$1")"
  encoded="$(urlencode_component "$playlist")"
  printf 'https://thumbnails.libretro.com/%s/%s\n' "$encoded" "$KIND"
}

lookup_crc_name() {
  local index="$1"
  local crc="$2"
  awk -F '\t' -v crc="$crc" '$1 == crc { print $2; exit }' "$index"
}

name_exists_in_index() {
  local names="$1"
  local name="$2"
  grep -Fqx -- "${name}.png" "$names"
}

loose_name_from_index() {
  local names="$1"
  local wanted="$2"
  python3 - "$names" "$wanted" <<'PY'
import re
import sys
import unicodedata

names_path, wanted = sys.argv[1], sys.argv[2]

def normalize_key(value):
    value = unicodedata.normalize("NFKC", value)
    value = re.sub(r"\.[A-Za-z0-9]{2,4}$", "", value)
    value = re.sub(r"\[[^\]]*\]", "", value)
    value = re.sub(r"\([^)]*\)", "", value)
    value = value.casefold()
    return "".join(ch for ch in value if ch.isalnum())

wanted_key = normalize_key(wanted)
if not wanted_key:
    sys.exit(1)

with open(names_path, encoding="utf-8") as handle:
    for line in handle:
        name = line.rstrip("\n")
        base = name[:-4] if name.lower().endswith(".png") else name
        if normalize_key(base) == wanted_key:
            print(base)
            sys.exit(0)
sys.exit(1)
PY
}

is_png_file() {
  local file="$1"
  local sig
  sig="$(head -c 8 "$file" | od -An -tx1 | tr -d ' \n')"
  [ "$sig" = "89504e470d0a1a0a" ]
}

thumb_url_for_name() {
  local system="$1" name="$2" base encoded
  base="$(thumb_base_url "$system")"
  encoded="$(urlencode_component "${name}.png")"
  printf '%s/%s\n' "$base" "$encoded"
}

remote_png_exists() {
  local url="$1"
  curl -fsIL --retry 1 --connect-timeout 5 --max-time 12 "$url" >/dev/null 2>&1
}

choose_thumbnail_name() {
  local system="$1"
  local canonical="$2"
  local rom_stem="$3"
  local names match url

  if [ "$LOOSE_INDEX" -eq 1 ] || [ -n "$CANDIDATE_REPORT_PATH" ]; then
    names="$(ensure_thumb_names "$system")"
  fi

  if [ -n "$canonical" ]; then
    if [ -n "$names" ] && name_exists_in_index "$names" "$canonical"; then
      printf 'crc-exact\t%s\n' "$canonical"
      return 0
    fi
    if [ -z "$names" ] && url="$(thumb_url_for_name "$system" "$canonical")" && remote_png_exists "$url"; then
      printf 'crc-exact\t%s\n' "$canonical"
      return 0
    fi
  fi
  if [ -n "$rom_stem" ] && [ "$rom_stem" != "$canonical" ]; then
    if [ -n "$names" ] && name_exists_in_index "$names" "$rom_stem"; then
      printf 'name-exact\t%s\n' "$rom_stem"
      return 0
    fi
    if [ -z "$names" ] && url="$(thumb_url_for_name "$system" "$rom_stem")" && remote_png_exists "$url"; then
      printf 'name-exact\t%s\n' "$rom_stem"
      return 0
    fi
  fi

  [ "$LOOSE_INDEX" -eq 1 ] || return 1
  [ -n "$names" ] || names="$(ensure_thumb_names "$system")"
  if [ -n "$canonical" ] && match="$(loose_name_from_index "$names" "$canonical")"; then
    printf 'crc-loose\t%s\n' "$match"
    return 0
  fi
  if [ -n "$rom_stem" ] && match="$(loose_name_from_index "$names" "$rom_stem")"; then
    printf 'name-loose\t%s\n' "$match"
    return 0
  fi
  return 1
}

candidate_names_from_index() {
  local names="$1"
  local canonical="$2"
  local rom_stem="$3"
  python3 - "$names" "$canonical" "$rom_stem" "$CANDIDATE_LIMIT" "$CANDIDATE_MIN_SCORE" <<'PY'
import difflib
import re
import sys
import unicodedata

names_path, canonical, rom_stem = sys.argv[1], sys.argv[2], sys.argv[3]
limit, min_score = int(sys.argv[4]), int(sys.argv[5])

def strip_noise(value):
    value = unicodedata.normalize("NFKC", value)
    value = re.sub(r"\.[A-Za-z0-9]{2,4}$", "", value)
    value = re.sub(r"\[[^\]]*\]", " ", value)
    value = re.sub(r"\([^)]*\)", " ", value)
    value = re.sub(r"[_\-:;,.!?/\\+&'’]+", " ", value)
    value = re.sub(r"([A-Za-z])([0-9])", r"\1 \2", value)
    value = re.sub(r"([0-9])([A-Za-z])", r"\1 \2", value)
    return value.casefold()

def tokens_for(value):
    value = strip_noise(value)
    return re.findall(r"[a-z0-9]+|[\u3040-\u30ff\u3400-\u9fff]+", value)

def compact(tokens):
    return "".join(tokens)

def dedupe_key(value):
    return compact(tokens_for(value))

def quality_for(value):
    penalty = 0
    if "[" in value or "]" in value:
        penalty += 20
    if re.search(r"\((?:19|20)\d\d", value):
        penalty += 5
    if re.search(r"\((?:JP|US|EU|AS)\)", value):
        penalty += 2
    return -penalty

def has_rough_match(query_tokens, candidate_tokens, query_key, candidate_key):
    if query_key in candidate_key or candidate_key in query_key:
        return True
    if set(query_tokens) & set(candidate_tokens):
        return True
    for token in query_tokens:
        if len(token) >= 4 and token in candidate_key:
            return True
    for token in candidate_tokens:
        if len(token) >= 4 and token in query_key:
            return True
    return False

def score(query, candidate):
    query_tokens = tokens_for(query)
    candidate_tokens = tokens_for(candidate)
    query_key = compact(query_tokens)
    candidate_key = compact(candidate_tokens)
    if not query_key or not candidate_key:
        return 0

    if query_key == candidate_key:
        return 100

    if not has_rough_match(query_tokens, candidate_tokens, query_key, candidate_key):
        return 0

    contain = 0
    if query_key in candidate_key:
        contain = min(96, 70 + int(30 * len(query_key) / max(len(candidate_key), 1)))
    elif candidate_key in query_key:
        contain = min(90, 58 + int(32 * len(candidate_key) / max(len(query_key), 1)))

    query_set = set(query_tokens)
    candidate_set = set(candidate_tokens)
    overlap = query_set & candidate_set
    token_score = 0
    if overlap:
        query_weight = sum(len(token) for token in query_tokens)
        overlap_weight = sum(len(token) for token in query_tokens if token in overlap)
        coverage = overlap_weight / max(query_weight, 1)
        precision = len(overlap) / max(len(candidate_set), 1)
        token_score = int(round((coverage * 0.75 + precision * 0.25) * 100))

    ratio = int(round(difflib.SequenceMatcher(None, query_key, candidate_key).ratio() * 100))

    best = max(ratio, contain, token_score)
    query_numbers = {token for token in query_tokens if token.isdigit()}
    candidate_numbers = {token for token in candidate_tokens if token.isdigit()}
    if query_numbers and not query_numbers <= candidate_numbers:
        best = min(best, 64)
    return best

queries = []
if canonical:
    queries.append(("crc-canonical", canonical))
if rom_stem and rom_stem != canonical:
    queries.append(("rom-stem", rom_stem))

if not queries:
    sys.exit(0)

best_by_key = {}
with open(names_path, encoding="utf-8") as handle:
    for line in handle:
        name = line.rstrip("\n")
        if not name.lower().endswith(".png"):
            continue
        base = name[:-4]
        key = dedupe_key(base)
        if not key:
            continue
        for source, query in queries:
            candidate_score = score(query, base)
            if candidate_score < min_score:
                continue
            current = best_by_key.get(key)
            next_value = (candidate_score, quality_for(base), source, query, base)
            if current is None or (candidate_score, quality_for(base), source) > (current[0], current[1], current[2]):
                best_by_key[key] = next_value

ranked = sorted(
    best_by_key.values(),
    key=lambda item: (-item[0], -item[1], item[2] != "crc-canonical", item[4].casefold()),
)
for candidate_score, quality, source, query, base in ranked[:limit]:
    print(f"{source}\t{candidate_score}\t{query}\t{base}")
PY
}

candidate_report_line() {
  local line="$*"
  line="${line//\\t/$'\t'}"
  [ -n "$CANDIDATE_REPORT_PATH" ] || return 0
  mkdir -p "$(dirname "$CANDIDATE_REPORT_PATH")"
  printf '%s\n' "$line" >>"$CANDIDATE_REPORT_PATH"
}

report_candidates() {
  local system="$1" crc="$2" rom="$3" dest="$4" canonical="$5" rom_stem="$6"
  local names source score query thumb_name url

  [ -n "$CANDIDATE_REPORT_PATH" ] || return 0
  names="$(ensure_thumb_names "$system")"
  while IFS=$'\t' read -r source score query thumb_name; do
    [ -n "$thumb_name" ] || continue
    url="$(thumb_url_for_name "$system" "$thumb_name")"
    candidate_report_line "candidate\t$system\t$source\t$score\t$crc\t$rom\t$dest\t$query\t${thumb_name}.png\t$url"
  done < <(candidate_names_from_index "$names" "$canonical" "$rom_stem")
}

report_line() {
  local line="$*"
  line="${line//\\t/$'\t'}"
  printf '%s\n' "$line"
  if [ -n "$REPORT_PATH" ]; then
    mkdir -p "$(dirname "$REPORT_PATH")"
    printf '%s\n' "$line" >>"$REPORT_PATH"
  fi
}

negative_key() {
  local system="$1" rel="$2" size="$3" mtime="$4"
  printf '%s\t%s\t%s\t%s\t%s\n' "$system" "$KIND" "$rel" "$size" "$mtime"
}

negative_seen() {
  local key="$1"
  local negative="${CACHE_ROOT}/negative.tsv"
  [ "$RETRY_NEGATIVE" -eq 0 ] || return 1
  [ -s "$negative" ] || return 1
  grep -Fqx -- "$key" "$negative"
}

negative_add() {
  local key="$1"
  local negative="${CACHE_ROOT}/negative.tsv"
  mkdir -p "$(dirname "$negative")"
  grep -Fqx -- "$key" "$negative" 2>/dev/null || printf '%s\n' "$key" >>"$negative"
}

first_supported_zip_member() {
  local zip="$1"
  zipinfo -1 "$zip" | while IFS= read -r member; do
    local ext
    ext="$(lower_ext "$member" || true)"
    case "$ext" in
      nes|fds|gb) printf '%s\n' "$member"; return 0 ;;
    esac
  done
}

process_rom() {
  local rom="$1"
  local source_system="$2"
  local root payload payload_tmp ext system rel output_rel dest rom_stem existing_thumb identity size mtime crc
  local dat_index canonical choice method thumb_name url key tmp

  payload="$rom"
  payload_tmp=""
  ext="$(lower_ext "$rom" || true)"
  if [ "$ext" = "zip" ]; then
    local member member_ext
    member="$(first_supported_zip_member "$rom" || true)"
    if [ -z "$member" ]; then
      report_line "skip_zip\t-\t-\t-\t$rom\t-\t-\t-"
      return 0
    fi
    member_ext="$(lower_ext "$member")"
    system="$(system_for_ext "$member_ext")"
  else
    system="$(system_for_ext "$ext" 2>/dev/null || true)"
    if [ -z "$system" ]; then
      report_line "skip_ext\t-\t-\t-\t$rom\t-\t-\t-"
      return 0
    fi
  fi

  if [ "$SYSTEM_FILTER" != "all" ] && [ "$SYSTEM_FILTER" != "$system" ]; then
    [ -z "$payload_tmp" ] || rm -f "$payload_tmp"
    return 0
  fi
  if [ "$source_system" = "gb" ] && [ "$system" != "gb" ]; then
    [ -z "$payload_tmp" ] || rm -f "$payload_tmp"
    return 0
  fi

  root="$(system_rom_root "$source_system")"
  rel="$(relative_to_root "$root" "$rom")"
  output_rel="$(strip_ext "$rel")"
  rom_stem="$(basename "$output_rel")"
  dest="${IMAGE_ROOT%/}/${system}/${output_rel}.png"

  existing_thumb="$(existing_thumbnail_path "$system" "$output_rel" "$rom_stem" || true)"
  if [ -n "$existing_thumb" ]; then
    report_line "exists\t$system\tlocal\t-\t$rom\t$existing_thumb\t-\t-"
    return 0
  fi

  identity="$(file_identity "$rom")"
  size="${identity%%$'\t'*}"
  mtime="${identity#*$'\t'}"
  key="$(negative_key "$system" "$rel" "$size" "$mtime")"
  if negative_seen "$key"; then
    report_line "negative_cached\t$system\t-\t-\t$rom\t$dest\t-\t-"
    return 0
  fi

  if [ "$ext" = "zip" ]; then
    payload_tmp="$(mktemp "${TMPDIR:-/tmp}/plumos-rom.XXXXXX")"
    unzip -p "$rom" "$member" >"$payload_tmp"
    payload="$payload_tmp"
  fi

  crc="$(crc32 "$payload" | tr '[:upper:]' '[:lower:]')"
  [ -z "$payload_tmp" ] || rm -f "$payload_tmp"

  dat_index="$(ensure_dat_index "$system")"
  canonical="$(lookup_crc_name "$dat_index" "$crc" || true)"
  choice="$(choose_thumbnail_name "$system" "$canonical" "$rom_stem" || true)"
  if [ -z "$choice" ]; then
    report_candidates "$system" "$crc" "$rom" "$dest" "$canonical" "$rom_stem"
    [ "$MODE" = "fetch" ] && negative_add "$key"
    report_line "no_match\t$system\t-\t$crc\t$rom\t$dest\t-\t-"
    return 0
  fi

  method="${choice%%$'\t'*}"
  thumb_name="${choice#*$'\t'}"
  url="$(thumb_url_for_name "$system" "$thumb_name")"

  if [ "$MODE" = "dry-run" ]; then
    report_line "would_download\t$system\t$method\t$crc\t$rom\t$dest\t${thumb_name}.png\t$url"
    return 0
  fi

  mkdir -p "$(dirname "$dest")"
  tmp="${dest}.tmp.$$"
  if ! curl -fsSL --retry 2 --connect-timeout 10 --max-time 60 -o "$tmp" "$url"; then
    rm -f "$tmp"
    negative_add "$key"
    report_line "download_failed\t$system\t$method\t$crc\t$rom\t$dest\t${thumb_name}.png\t$url"
    return 0
  fi
  if ! is_png_file "$tmp"; then
    rm -f "$tmp"
    negative_add "$key"
    report_line "invalid_png\t$system\t$method\t$crc\t$rom\t$dest\t${thumb_name}.png\t$url"
    return 0
  fi
  mv "$tmp" "$dest"
  report_line "downloaded\t$system\t$method\t$crc\t$rom\t$dest\t${thumb_name}.png\t$url"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --fetch) MODE="fetch"; shift ;;
    --dry-run) MODE="dry-run"; shift ;;
    --system) SYSTEM_FILTER="${2:-}"; shift 2 ;;
    --kind) KIND="${2:-}"; shift 2 ;;
    --nes-dir) NES_ROM_DIR="${2:-}"; shift 2 ;;
    --gb-dir) GB_ROM_DIR="${2:-}"; shift 2 ;;
    --image-root) IMAGE_ROOT="${2:-}"; shift 2 ;;
    --cache-root) CACHE_ROOT="${2:-}"; shift 2 ;;
    --report) REPORT_PATH="${2:-}"; rm -f "$REPORT_PATH"; shift 2 ;;
    --candidate-report) CANDIDATE_REPORT_PATH="${2:-}"; rm -f "$CANDIDATE_REPORT_PATH"; shift 2 ;;
    --candidate-limit) CANDIDATE_LIMIT="${2:-5}"; shift 2 ;;
    --candidate-min-score) CANDIDATE_MIN_SCORE="${2:-60}"; shift 2 ;;
    --limit) LIMIT="${2:-0}"; shift 2 ;;
    --retry-negative) RETRY_NEGATIVE=1; shift ;;
    --loose-index) LOOSE_INDEX=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

case "$KIND" in
  Named_Boxarts|Named_Snaps|Named_Titles) ;;
  *) die "invalid --kind: $KIND" ;;
esac
case "$SYSTEM_FILTER" in
  all|nes|fds|gb) ;;
  *) die "invalid --system: $SYSTEM_FILTER" ;;
esac
[[ "$CANDIDATE_LIMIT" =~ ^[0-9]+$ ]] || die "invalid --candidate-limit: $CANDIDATE_LIMIT"
[[ "$CANDIDATE_MIN_SCORE" =~ ^[0-9]+$ ]] || die "invalid --candidate-min-score: $CANDIDATE_MIN_SCORE"
[ "$CANDIDATE_LIMIT" -gt 0 ] || die "--candidate-limit must be greater than 0"
[ "$CANDIDATE_MIN_SCORE" -le 100 ] || die "--candidate-min-score must be 0..100"

need_cmd awk
need_cmd crc32
need_cmd curl
need_cmd grep
need_cmd head
need_cmd od
need_cmd python3
need_cmd sed
need_cmd unzip
need_cmd zipinfo

mkdir -p "$CACHE_ROOT"
printf 'mode=%s kind=%s loose_index=%s image_root=%s cache_root=%s\n' \
  "$MODE" "$KIND" "$LOOSE_INDEX" "$IMAGE_ROOT" "$CACHE_ROOT" >&2
report_line "status\tsystem\tmethod\tcrc\trom_path\tdest_path\tthumbnail_name\turl"
if [ -n "$CANDIDATE_REPORT_PATH" ]; then
  candidate_report_line "status\tsystem\tsource\tscore\tcrc\trom_path\tdest_path\tquery_name\tthumbnail_name\turl"
fi

processed=0
case "$SYSTEM_FILTER" in
  gb) source_systems="gb" ;;
  nes|fds) source_systems="nes" ;;
  all) source_systems="nes gb" ;;
esac

for source_system in $source_systems; do
  root="$(system_rom_root "$source_system")"
  [ -d "$root" ] || continue
  find_args=(-type f)
  case "$SYSTEM_FILTER" in
    nes) find_args+=(\( -iname '*.nes' -o -iname '*.zip' \)) ;;
    fds) find_args+=(\( -iname '*.fds' \)) ;;
    gb) find_args+=(\( -iname '*.gb' \)) ;;
    all) find_args+=(\( -iname '*.nes' -o -iname '*.fds' -o -iname '*.gb' -o -iname '*.zip' \)) ;;
  esac
  while IFS= read -r -d '' rom; do
    process_rom "$rom" "$source_system"
    processed=$((processed + 1))
    if [ "$LIMIT" -gt 0 ] && [ "$processed" -ge "$LIMIT" ]; then
      exit 0
    fi
  done < <(find "$root" "${find_args[@]}" -print0)
done
