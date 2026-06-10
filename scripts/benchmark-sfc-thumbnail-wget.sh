#!/bin/sh
# Extract SFC thumbnail URLs and benchmark sequential wget downloads.

set -u

TAB_CHAR="$(printf '\t')"
SCRIPT_NAME="${0##*/}"

SDCARD_ROOT="${PLUMOS_SDCARD_ROOT:-/mnt/SDCARD}"
SDCARD_ROOT="${SDCARD_ROOT%/}"
PLUMOS_ROOT="${PLUMOS_ROOT:-${SDCARD_ROOT}/plumos}"
PATH="${PLUMOS_ROOT}/bin:${PLUMOS_ROOT}/gnu/bin:${PATH}"
export PATH

SYSTEM_ID=sfc
KIND="${PLUMOS_THUMBNAIL_KIND:-Named_Boxarts}"
SYSTEM_ALIASES="${PLUMOS_SFC_ALIASES:-SFC sfc snes}"
SYSTEM_EXTENSIONS="${PLUMOS_SFC_EXTENSIONS:-sfc smc fig bs swc zip}"
DEFAULT_PLAYLIST="Nintendo - Super Nintendo Entertainment System"
DEFAULT_DAT_PATH="metadat/no-intro/Nintendo - Super Nintendo Entertainment System.dat"

if [ -n "${PLUMOS_ROM_ROOT:-}" ]; then
  ROM_ROOTS="${PLUMOS_ROM_ROOT%/}"
else
  ROM_ROOTS="${SDCARD_ROOT}/Roms
${SDCARD_ROOT}/roms"
fi

IMAGE_ROOT="${PLUMOS_IMAGE_ROOT:-${SDCARD_ROOT}/Images}"
IMAGE_ROOT="${IMAGE_ROOT%/}"
SOURCES_TSV="${PLUMOS_SCRAPER_SOURCES:-${PLUMOS_ROOT}/config/frontend/scraper-sources.tsv}"
STATE_DIR="${PLUMOS_THUMBNAIL_STATE_DIR:-${PLUMOS_ROOT}/state/frontend/artwork-scraper}"
CACHE_DIR="${PLUMOS_THUMBNAIL_CACHE_DIR:-${PLUMOS_ROOT}/cache/frontend/artwork-scraper}"
PRELOAD_DIR="${PLUMOS_THUMBNAIL_PRELOAD_DIR:-${PLUMOS_ROOT}/share/frontend/artwork-scraper}"
FETCH_TIMEOUT="${PLUMOS_THUMBNAIL_FETCH_TIMEOUT:-12}"
FETCH_RETRY="${PLUMOS_THUMBNAIL_FETCH_RETRY:-0}"

usage() {
  cat <<USAGE
Usage:
  ${SCRIPT_NAME} extract OUT.tsv [--missing-only|--all] [--limit N]
  ${SCRIPT_NAME} bench URLS.tsv OUT.tsv [--limit N] [--timeout SEC] [--keep-files]

Examples:
  ${SCRIPT_NAME} extract /tmp/sfc-urls.tsv --missing-only
  ${SCRIPT_NAME} bench /tmp/sfc-urls.tsv /tmp/sfc-wget.tsv --timeout 12

Notes:
  extract resolves SFC ROM CRCs to libretro thumbnail URLs without downloading PNGs.
  bench downloads each URL sequentially with wget into a temporary directory.
USAGE
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

is_positive_int_or_zero() {
  case "$1" in
    ''|*[!0-9]*) return 1 ;;
  esac
  return 0
}

lower_ext() {
  file_name="${1##*/}"
  case "$file_name" in
    *.*) printf '%s\n' "${file_name##*.}" | tr '[:upper:]' '[:lower:]' ;;
    *) return 1 ;;
  esac
}

extension_allowed() {
  wanted="$1"
  allowed_list="$2"
  for allowed in $allowed_list; do
    [ "$wanted" = "$allowed" ] && return 0
  done
  return 1
}

thumbnail_lookup_key() {
  printf '%s\n' "$1" | tr '[:upper:]' '[:lower:]'
}

thumbnail_lookup_add() {
  lookup_file="$1"
  stem="$2"
  [ -n "$stem" ] || return 0
  key="$(thumbnail_lookup_key "$stem")"
  grep -Fqx -- "$key" "$lookup_file" 2>/dev/null || printf '%s\n' "$key" >> "$lookup_file"
}

build_thumbnail_lookup() {
  lookup_file="$1"
  thumb_root="${IMAGE_ROOT}/${SYSTEM_ID}"

  rm -f "$lookup_file" 2>/dev/null || true
  : > "$lookup_file"
  [ -d "$thumb_root" ] || return 0

  find "$thumb_root" -type f 2>/dev/null | while IFS= read -r thumb_path; do
    [ -s "$thumb_path" ] || continue
    image_ext="$(lower_ext "$thumb_path" 2>/dev/null || true)"
    case "$image_ext" in
      png|jpg|jpeg|webp) ;;
      *) continue ;;
    esac
    rel_path="${thumb_path#${thumb_root}/}"
    thumbnail_lookup_add "$lookup_file" "${rel_path%.*}"
  done
}

thumbnail_exists() {
  lookup_file="$1"
  rel_stem="$2"
  flat_stem="$3"
  [ -s "$lookup_file" ] || return 1

  for stem in "$rel_stem" "$flat_stem"; do
    [ -n "$stem" ] || continue
    key="$(thumbnail_lookup_key "$stem")"
    grep -Fqx -- "$key" "$lookup_file" 2>/dev/null && return 0
  done
  return 1
}

dir_identity() {
  if [ -d "$1" ]; then
    inode="$(ls -id "$1" 2>/dev/null | awk '{ print $1 }')"
    if [ -n "$inode" ]; then
      printf 'inode:%s\n' "$inode"
      return 0
    fi
    (cd "$1" 2>/dev/null && pwd -P) || printf '%s\n' "$1"
  else
    printf '%s\n' "$1"
  fi
}

source_field() {
  field="$1"
  [ -r "$SOURCES_TSV" ] || return 1
  awk -F "$TAB_CHAR" -v target="$SYSTEM_ID" -v field="$field" '
    $1 == target {
      print $field
      exit
    }
  ' "$SOURCES_TSV"
}

source_playlist() {
  value="$(source_field 2 2>/dev/null || true)"
  [ -n "$value" ] || value="$DEFAULT_PLAYLIST"
  printf '%s\n' "$value"
}

source_dat_path() {
  value="$(source_field 3 2>/dev/null || true)"
  [ -n "$value" ] || value="$DEFAULT_DAT_PATH"
  printf '%s\n' "$value"
}

space_encode() {
  printf '%s' "$1" | sed 's/ /%20/g'
}

dat_source_url() {
  dat_path="$(source_dat_path)"
  printf 'https://raw.githubusercontent.com/libretro/libretro-database/master/%s\n' \
    "$(space_encode "$dat_path")"
}

thumb_base_url() {
  playlist="$(source_playlist)"
  printf 'https://thumbnails.libretro.com/%s/%s\n' "$(space_encode "$playlist")" "$KIND"
}

fetch_url() {
  url="$1"
  output="$2"
  tmp="${output}.tmp.$$"

  rm -f "$tmp" 2>/dev/null || true
  mkdir -p "$(dirname "$output")" 2>/dev/null || true
  if command -v curl >/dev/null 2>&1; then
    if curl -fsSL --retry "$FETCH_RETRY" --connect-timeout 10 --max-time "$FETCH_TIMEOUT" \
      -o "$tmp" "$url"; then
      mv "$tmp" "$output"
      return 0
    fi
  elif command -v wget >/dev/null 2>&1; then
    if wget -q -T "$FETCH_TIMEOUT" -O "$tmp" "$url"; then
      mv "$tmp" "$output"
      return 0
    fi
  else
    return 127
  fi

  rm -f "$tmp" 2>/dev/null || true
  return 1
}

dat_to_crc_index() {
  dat_file="$1"
  index_file="$2"
  awk '
    /^game[[:space:]]*\(/ {
      game = ""
      next
    }
    /^[[:space:]]*name[[:space:]]+"/ && game == "" {
      line = $0
      sub(/^[[:space:]]*name[[:space:]]+"/, "", line)
      sub(/"$/, "", line)
      game = line
      next
    }
    /^[[:space:]]*rom[[:space:]]*\(/ && game != "" {
      if (match($0, /crc[[:space:]][0-9A-Fa-f]+/)) {
        crc = substr($0, RSTART + 4, RLENGTH - 4)
        gsub(/[[:space:]]/, "", crc)
        print tolower(crc) "\t" game
      }
    }
  ' "$dat_file" > "$index_file"
}

html_to_thumb_index() {
  html_file="$1"
  index_file="$2"
  awk '
    /<a[[:space:]][^>]*href="[^"]+\.png"/ {
      line = $0
      href = line
      sub(/^.*<a[[:space:]][^>]*href="/, "", href)
      sub(/".*$/, "", href)
      label = line
      sub(/^.*href="[^"]+">/, "", label)
      sub(/<\/a>.*$/, "", label)
      sub(/\.png$/, "", label)
      gsub(/&amp;/, "\\&", label)
      gsub(/&quot;/, "\"", label)
      gsub(/&#39;/, sprintf("%c", 39), label)
      if (href !~ /^\// && label != "") {
        print label "\t" href
      }
    }
  ' "$html_file" > "$index_file"
}

thumb_index_valid() {
  index_file="$1"
  [ -s "$index_file" ] || return 1
  awk -F "$TAB_CHAR" '
    NR > 20 { exit }
    $1 ~ /[<>]/ { bad = 1; exit }
    END { exit bad ? 1 : 0 }
  ' "$index_file"
}

ensure_dat_index() {
  preloaded="${PRELOAD_DIR}/dat/${SYSTEM_ID}.crc.tsv"
  cached="${CACHE_DIR}/dat/${SYSTEM_ID}.crc.tsv"
  source_dat="${CACHE_DIR}/dat-source/${SYSTEM_ID}.dat"

  if [ -s "$preloaded" ]; then
    printf '%s\n' "$preloaded"
    return 0
  fi
  if [ -s "$cached" ]; then
    printf '%s\n' "$cached"
    return 0
  fi

  mkdir -p "${CACHE_DIR}/dat" "${CACHE_DIR}/dat-source" 2>/dev/null || true
  fetch_url "$(dat_source_url)" "$source_dat" || return 1
  dat_to_crc_index "$source_dat" "${cached}.tmp" || return 1
  mv "${cached}.tmp" "$cached"
  printf '%s\n' "$cached"
}

ensure_thumb_index() {
  preloaded="${PRELOAD_DIR}/thumb-names/${SYSTEM_ID}/${KIND}.tsv"
  cached="${CACHE_DIR}/thumb-names/${SYSTEM_ID}/${KIND}.tsv"
  html="${CACHE_DIR}/thumb-html/${SYSTEM_ID}/${KIND}.html"
  base="$(thumb_base_url)"

  if thumb_index_valid "$preloaded"; then
    printf '%s\n' "$preloaded"
    return 0
  fi
  if thumb_index_valid "$cached"; then
    printf '%s\n' "$cached"
    return 0
  fi

  mkdir -p "${CACHE_DIR}/thumb-names/${SYSTEM_ID}" "${CACHE_DIR}/thumb-html/${SYSTEM_ID}" \
    2>/dev/null || true
  fetch_url "${base}/" "$html" || return 1
  html_to_thumb_index "$html" "${cached}.tmp" || return 1
  mv "${cached}.tmp" "$cached"
  printf '%s\n' "$cached"
}

lookup_crc_name() {
  index="$1"
  crc="$2"
  awk -F "$TAB_CHAR" -v crc="$crc" '$1 == crc { print $2; exit }' "$index"
}

lookup_thumb_href() {
  index="$1"
  name="$2"
  awk -F "$TAB_CHAR" -v name="$name" '$1 == name { print $2; exit }' "$index"
}

compute_crc32() {
  crc32 "$1" 2>/dev/null | awk '{ print tolower($1); exit }'
}

first_zip_member() {
  zip_path="$1"
  allowed_list="$2"
  command -v unzip >/dev/null 2>&1 || return 1
  unzip -lq "$zip_path" 2>/dev/null | while IFS= read -r member; do
    case "$member" in
      ""|*/|*"/") continue ;;
    esac
    ext="$(lower_ext "$member" 2>/dev/null || true)"
    [ -n "$ext" ] || continue
    case "$ext" in
      zip|7z) continue ;;
    esac
    extension_allowed "$ext" "$allowed_list" || continue
    printf '%s\n' "$member"
    return 0
  done
}

now_ms() {
  if [ -r /proc/uptime ]; then
    awk '{ printf "%.0f\n", $1 * 1000 }' /proc/uptime
    return
  fi
  if command -v gdate >/dev/null 2>&1; then
    gdate +%s%3N
    return
  fi
  if command -v perl >/dev/null 2>&1; then
    perl -MTime::HiRes=time -e 'printf "%.0f\n", time() * 1000'
    return
  fi
  printf '%s000\n' "$(date +%s)"
}

file_size() {
  path="$1"
  [ -s "$path" ] || {
    printf '0\n'
    return
  }
  wc -c < "$path" | tr -d ' '
}

extract_urls() {
  out_path="$1"
  missing_only="$2"
  limit="$3"
  dat_index="$(ensure_dat_index 2>/dev/null || true)"
  thumb_index="$(ensure_thumb_index 2>/dev/null || true)"
  thumb_base="$(thumb_base_url)"
  thumb_lookup="${STATE_DIR}/benchmark-thumbnails-${SYSTEM_ID}-$$.list"
  tmp_list="${STATE_DIR}/benchmark-roms-${SYSTEM_ID}-$$.list"
  payload_tmp="${STATE_DIR}/benchmark-payload-${SYSTEM_ID}-$$.rom"
  seen_dirs=" "
  candidates=0
  existing_count=0
  urls=0
  crc_miss=0
  thumbnail_miss=0
  skipped_zip=0
  skipped_tool=0
  stop_scan=0

  [ -n "$dat_index" ] || die "cannot prepare DAT CRC index"
  [ -n "$thumb_index" ] || die "cannot prepare thumbnail index"
  mkdir -p "$STATE_DIR" "$(dirname "$out_path")" 2>/dev/null || true
  build_thumbnail_lookup "$thumb_lookup"

  printf 'system\trom_path\tcrc\tcanonical\turl\texisting_thumbnail\n' > "$out_path"

  for rom_root in $ROM_ROOTS; do
    for alias in $SYSTEM_ALIASES; do
      alias_dir="${rom_root%/}/${alias}"
      [ -d "$alias_dir" ] || continue
      identity="$(dir_identity "$alias_dir")"
      case "$seen_dirs" in
        *" ${identity} "*) continue ;;
      esac
      seen_dirs="${seen_dirs}${identity} "

      find "$alias_dir" -type f > "$tmp_list" 2>/dev/null || true
      while IFS= read -r rom_path; do
        ext="$(lower_ext "$rom_path" 2>/dev/null || true)"
        [ -n "$ext" ] || continue
        extension_allowed "$ext" "$SYSTEM_EXTENSIONS" || continue

        candidates=$((candidates + 1))
        rel_path="${rom_path#"$alias_dir"/}"
        rel_stem="${rel_path%.*}"
        flat_stem="${rel_stem##*/}"
        existing=0
        if thumbnail_exists "$thumb_lookup" "$rel_stem" "$flat_stem"; then
          existing=1
          existing_count=$((existing_count + 1))
        fi
        if [ "$missing_only" = 1 ] && [ "$existing" = 1 ]; then
          if [ "$limit" -gt 0 ] && [ "$candidates" -ge "$limit" ]; then
            stop_scan=1
            break
          fi
          continue
        fi

        payload="$rom_path"
        rm -f "$payload_tmp" 2>/dev/null || true
        if [ "$ext" = "zip" ]; then
          member="$(first_zip_member "$rom_path" "$SYSTEM_EXTENSIONS" || true)"
          if [ -z "$member" ] || ! unzip -p "$rom_path" "$member" > "$payload_tmp" 2>/dev/null; then
            skipped_zip=$((skipped_zip + 1))
            if [ "$limit" -gt 0 ] && [ "$candidates" -ge "$limit" ]; then
              stop_scan=1
              break
            fi
            continue
          fi
          payload="$payload_tmp"
        fi

        crc="$(compute_crc32 "$payload" || true)"
        rm -f "$payload_tmp" 2>/dev/null || true
        if [ -z "$crc" ]; then
          skipped_tool=$((skipped_tool + 1))
        else
          canonical="$(lookup_crc_name "$dat_index" "$crc" || true)"
          if [ -z "$canonical" ]; then
            crc_miss=$((crc_miss + 1))
          else
            href="$(lookup_thumb_href "$thumb_index" "$canonical" || true)"
            if [ -z "$href" ]; then
              thumbnail_miss=$((thumbnail_miss + 1))
            else
              url="${thumb_base}/${href}"
              printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
                "$SYSTEM_ID" "$rom_path" "$crc" "$canonical" "$url" "$existing" >> "$out_path"
              urls=$((urls + 1))
            fi
          fi
        fi

        if [ "$limit" -gt 0 ] && [ "$candidates" -ge "$limit" ]; then
          stop_scan=1
          break
        fi
      done < "$tmp_list"

      [ "$stop_scan" = 1 ] && break
    done
    [ "$stop_scan" = 1 ] && break
  done

  rm -f "$tmp_list" "$payload_tmp" "$thumb_lookup" 2>/dev/null || true
  printf 'summary system=%s candidates=%s existing=%s urls=%s crc_miss=%s thumbnail_miss=%s skipped_zip=%s skipped_tool=%s missing_only=%s output=%s\n' \
    "$SYSTEM_ID" "$candidates" "$existing_count" "$urls" "$crc_miss" "$thumbnail_miss" \
    "$skipped_zip" "$skipped_tool" "$missing_only" "$out_path" >&2
}

benchmark_urls() {
  in_path="$1"
  out_path="$2"
  limit="$3"
  timeout="$4"
  keep_files="$5"
  tmp_dir="${TMPDIR:-/tmp}/plumos-sfc-wget-bench-$$"
  index=0
  ok_count=0
  failed_count=0
  total_bytes=0
  total_start="$(now_ms)"

  [ -r "$in_path" ] || die "URL TSV is not readable: $in_path"
  command -v wget >/dev/null 2>&1 || die "wget is required"
  is_positive_int_or_zero "$timeout" || die "invalid timeout: $timeout"
  mkdir -p "$tmp_dir" "$(dirname "$out_path")" 2>/dev/null || true
  printf 'index\tstatus\telapsed_ms\tbytes\toutput_path\turl\n' > "$out_path"

  first=1
  while IFS="$TAB_CHAR" read -r system rom_path crc canonical url existing; do
    if [ "$first" = 1 ]; then
      first=0
      continue
    fi
    [ -n "$url" ] || continue
    next_index=$((index + 1))
    if [ "$limit" -gt 0 ] && [ "$next_index" -gt "$limit" ]; then
      break
    fi
    index="$next_index"

    out_file="${tmp_dir}/thumb-${index}.png"
    start_ms="$(now_ms)"
    if wget -q -T "$timeout" -O "$out_file" "$url"; then
      status=ok
      bytes="$(file_size "$out_file")"
      ok_count=$((ok_count + 1))
      total_bytes=$((total_bytes + bytes))
    else
      status=failed
      bytes=0
      failed_count=$((failed_count + 1))
      rm -f "$out_file" 2>/dev/null || true
    fi
    end_ms="$(now_ms)"
    elapsed_ms=$((end_ms - start_ms))
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$index" "$status" "$elapsed_ms" "$bytes" "$out_file" "$url" >> "$out_path"
    printf 'bench index=%s status=%s elapsed_ms=%s bytes=%s\n' \
      "$index" "$status" "$elapsed_ms" "$bytes" >&2
  done < "$in_path"

  total_end="$(now_ms)"
  if [ "$keep_files" != 1 ]; then
    rm -rf "$tmp_dir" 2>/dev/null || true
  fi
  printf 'summary urls=%s ok=%s failed=%s total_ms=%s total_bytes=%s output=%s keep_files=%s tmp_dir=%s\n' \
    "$index" "$ok_count" "$failed_count" "$((total_end - total_start))" "$total_bytes" \
    "$out_path" "$keep_files" "$tmp_dir" >&2
}

mode="${1:-}"
[ -n "$mode" ] || {
  usage
  exit 2
}
shift

case "$mode" in
  extract)
    out_path="${1:-}"
    [ -n "$out_path" ] || die "extract requires OUT.tsv"
    shift
    missing_only=1
    limit=0
    while [ "$#" -gt 0 ]; do
      case "$1" in
        --missing-only)
          missing_only=1
          ;;
        --all)
          missing_only=0
          ;;
        --limit)
          shift
          [ "$#" -gt 0 ] || die "--limit requires a value"
          is_positive_int_or_zero "$1" || die "invalid limit: $1"
          limit="$1"
          ;;
        -h|--help)
          usage
          exit 0
          ;;
        *)
          die "unknown extract option: $1"
          ;;
      esac
      shift
    done
    extract_urls "$out_path" "$missing_only" "$limit"
    ;;
  bench)
    in_path="${1:-}"
    out_path="${2:-}"
    [ -n "$in_path" ] || die "bench requires URLS.tsv"
    [ -n "$out_path" ] || die "bench requires OUT.tsv"
    shift 2
    limit=0
    timeout="$FETCH_TIMEOUT"
    keep_files=0
    while [ "$#" -gt 0 ]; do
      case "$1" in
        --limit)
          shift
          [ "$#" -gt 0 ] || die "--limit requires a value"
          is_positive_int_or_zero "$1" || die "invalid limit: $1"
          limit="$1"
          ;;
        --timeout)
          shift
          [ "$#" -gt 0 ] || die "--timeout requires a value"
          is_positive_int_or_zero "$1" || die "invalid timeout: $1"
          timeout="$1"
          ;;
        --keep-files)
          keep_files=1
          ;;
        -h|--help)
          usage
          exit 0
          ;;
        *)
          die "unknown bench option: $1"
          ;;
      esac
      shift
    done
    benchmark_urls "$in_path" "$out_path" "$limit" "$timeout" "$keep_files"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    die "unknown mode: $mode"
    ;;
esac
