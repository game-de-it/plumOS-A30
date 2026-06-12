#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ONION_DIR="${ONION_DIR:-${ROOT_DIR}/artifacts/onion/Onion}"
BUILDER_DIR="${BUILDER_DIR:-${ROOT_DIR}/artifacts/onion/Miyoo-Mini-Retroarch-builder}"
RECIPE_FILE="${RECIPE_FILE:-${ROOT_DIR}/docker/plumos-toolchain/libretro-core-recipes.tsv}"

usage() {
  cat <<EOF
Usage: ONION_DIR=/path/to/Onion $0

Inventories Onion prebuilt libretro cores, Onion launch references, and plumOS
core recipes. This is an audit helper; it does not download or build anything.

Defaults:
  ONION_DIR    ${ONION_DIR}
  BUILDER_DIR  ${BUILDER_DIR}
  RECIPE_FILE  ${RECIPE_FILE}
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

CORE_DIR="${ONION_DIR}/static/build/RetroArch/.retroarch/cores"
if [ ! -d "${CORE_DIR}" ]; then
  echo "error: Onion core directory not found: ${CORE_DIR}" >&2
  exit 1
fi
if [ ! -f "${RECIPE_FILE}" ]; then
  echo "error: recipe file not found: ${RECIPE_FILE}" >&2
  exit 1
fi

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/plumos-onion-cores.XXXXXX")"
trap 'rm -rf "${tmpdir}"' EXIT

onion_cores="${tmpdir}/onion-cores"
recipe_ids="${tmpdir}/recipe-ids"
builder_ids="${tmpdir}/builder-ids"
launch_refs="${tmpdir}/launch-refs"

find "${CORE_DIR}" -maxdepth 1 -type f -name '*_libretro.so' -exec basename {} \; |
  sed 's/_libretro[.]so$//' |
  sort -u >"${onion_cores}"

awk -F'|' '
  /^[[:space:]]*($|#)/ { next }
  { print $1 }
' "${RECIPE_FILE}" | sort -u >"${recipe_ids}"

: >"${builder_ids}"
if [ -d "${BUILDER_DIR}" ]; then
  for recipe in "${BUILDER_DIR}"/cores-onionos "${BUILDER_DIR}"/cores-onionos-special; do
    [ -f "${recipe}" ] || continue
    awk '
      /^[[:space:]]*($|#)/ { next }
      { print $1 }
    ' "${recipe}" >>"${builder_ids}"
  done
  sort -u -o "${builder_ids}" "${builder_ids}"
fi

if command -v rg >/dev/null 2>&1; then
  rg --no-filename -No '[A-Za-z0-9_+.-]+_libretro[.]so' "${ONION_DIR}/static/packages" 2>/dev/null |
    sed 's/_libretro[.]so$//' |
    sort -u >"${launch_refs}" || true
else
  find "${ONION_DIR}/static/packages" -type f \( -name launch.sh -o -name '*.json' \) -print0 2>/dev/null |
    xargs -0 grep -Eho '[A-Za-z0-9_+.-]+_libretro[.]so' 2>/dev/null |
    sed 's/_libretro[.]so$//' |
    sort -u >"${launch_refs}" || true
fi
[ -f "${launch_refs}" ] || : >"${launch_refs}"

count_lines() {
  wc -l <"$1" | tr -d ' '
}

echo "onion_core_dir=${CORE_DIR}"
echo "recipe_file=${RECIPE_FILE}"
echo "onion_prebuilt_count=$(count_lines "${onion_cores}")"
echo "recipe_count=$(count_lines "${recipe_ids}")"
echo "builder_recipe_count=$(count_lines "${builder_ids}")"
echo "launch_ref_count=$(count_lines "${launch_refs}")"
echo

echo "[onion_prebuilt_missing_from_plumos_recipe]"
comm -23 "${onion_cores}" "${recipe_ids}" || true
echo

echo "[plumos_recipe_only_not_in_onion_prebuilt]"
comm -13 "${onion_cores}" "${recipe_ids}" || true
echo

echo "[onion_prebuilt_without_builder_recipe]"
if [ -s "${builder_ids}" ]; then
  comm -23 "${onion_cores}" "${builder_ids}" || true
else
  echo "builder_recipe_unavailable"
fi
echo

echo "[launch_refs_without_prebuilt]"
comm -23 "${launch_refs}" "${onion_cores}" || true
