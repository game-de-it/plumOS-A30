#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CORE_RECIPES="${CORE_RECIPES:-${ROOT_DIR}/docker/plumos-toolchain/libretro-core-recipes.tsv}"
OUT_ROOT="${OUT_ROOT:-${ROOT_DIR}/artifacts/libretro-core-option-probes}"
JOBS="${JOBS:-4}"
BUILD_JOB_FALLBACKS="${BUILD_JOB_FALLBACKS:-1}"
LIBRETRO_OPTIMIZATION_PROFILE="${LIBRETRO_OPTIMIZATION_PROFILE:-speed}"

usage() {
  cat <<EOF
Usage: $0 CORE [MAKE_ARGS...]

Build one libretro core with one candidate make_args at a time and write a
summary under artifacts/libretro-core-option-probes.

Pass __recipe__ as a MAKE_ARGS value to test the current recipe entry. When
MAKE_ARGS are omitted, a conservative candidate set is used:
  __recipe__
  platform=armv7-hardfloat-neon HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon
  platform=armv7-hardfloat-neon
  platform=classic_armv7_a7
  platform=armv HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon
  platform=armv
  platform=rpi2
  platform=unix-rpi2
  platform=unix

Environment:
  JOBS                         Parallel job count. Default: ${JOBS}
  BUILD_JOB_FALLBACKS          Same-condition retry jobs. Default: ${BUILD_JOB_FALLBACKS}
  LIBRETRO_OPTIMIZATION_PROFILE Build optimization profile. Default: ${LIBRETRO_OPTIMIZATION_PROFILE}
  OUT_ROOT                     Output root. Default: ${OUT_ROOT}
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

sanitize() {
  printf '%s' "$1" |
    tr '[:upper:]' '[:lower:]' |
    sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//; s/^$/recipe/'
}

recipe_args_for() {
  local core=$1
  awk -F'|' -v core="${core}" '
    /^[[:space:]]*($|#)/ { next }
    $1 == core { print $7; found = 1; exit }
    END { if (!found) exit 1 }
  ' "${CORE_RECIPES}"
}

manifest_value() {
  local manifest=$1
  local core=$2
  local key=$3
  awk -v section="[${core}]" -v key="${key}" '
    $0 == section { in_section = 1; next }
    in_section && /^\[/ { exit }
    in_section && index($0, key "=") == 1 {
      print substr($0, length(key) + 2)
      exit
    }
  ' "${manifest}"
}

has_pattern() {
  local file=$1
  local pattern=$2
  if [ -f "${file}" ] && grep -Eq -- "${pattern}" "${file}"; then
    printf 'yes'
  else
    printf 'no'
  fi
}

core=${1:-}
[ -n "${core}" ] || {
  usage >&2
  exit 1
}
shift

[ -f "${CORE_RECIPES}" ] || die "recipe file not found: ${CORE_RECIPES}"

recipe_args=$(recipe_args_for "${core}") || die "core not found in recipes: ${core}"

declare -a candidates=()
if [ "$#" -gt 0 ]; then
  candidates=("$@")
else
  candidates=(
    "__recipe__"
    "platform=armv7-hardfloat-neon HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon"
    "platform=armv7-hardfloat-neon"
    "platform=classic_armv7_a7"
    "platform=armv HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon"
    "platform=armv"
    "platform=rpi2"
    "platform=unix-rpi2"
    "platform=unix"
  )
fi

stamp=$(date +%Y%m%d-%H%M%S)
run_dir="${OUT_ROOT}/${stamp}-${core}"
mkdir -p "${run_dir}"
summary="${run_dir}/summary.tsv"
printf 'core\tcandidate\trequested_args\tstatus\tmake_args\tmake_jobs\topt_patch\topt_flags\toutput\tcross_cc\thost_cc\tresidual_o2\tofast\tlto\tneon\tcortex_a7\tjob_retry\tdir\n' >"${summary}"

printf '[probe-libretro] core=%s recipe_args=%s\n' "${core}" "${recipe_args}" >&2
printf '[probe-libretro] out=%s\n' "${run_dir}" >&2

index=0
for candidate in "${candidates[@]}"; do
  if [ "${candidate}" = "__recipe__" ]; then
    requested="${recipe_args}"
    override=""
    label="recipe"
  else
    requested="${candidate}"
    override="${candidate}"
    label=$(sanitize "${candidate}")
  fi

  candidate_dir="${run_dir}/$(printf '%02d-%s' "${index}" "${label}")"
  target_dir="${candidate_dir}/dist"
  mkdir -p "${candidate_dir}"

  printf '[probe-libretro] %s candidate=%s\n' "${core}" "${requested}" >&2

  status=failed
  if (
    cd "${ROOT_DIR}" &&
      TARGET_DIR="/workspace/${target_dir#${ROOT_DIR}/}" \
      PLUMOS_CORE_FILTER="${core}" \
      LIBRETRO_MAKE_ARGS_OVERRIDE="${override}" \
      LIBRETRO_OPTIMIZATION_PROFILE="${LIBRETRO_OPTIMIZATION_PROFILE}" \
      FAIL_ON_CORE_ERROR=1 \
      LIBRETRO_CORE_BUILD_CONCURRENCY=1 \
      JOBS="${JOBS}" \
      BUILD_JOB_FALLBACKS="${BUILD_JOB_FALLBACKS}" \
      ./scripts/docker-build.sh libretro-cores
  ) >"${candidate_dir}/build.stdout.log" 2>"${candidate_dir}/build.stderr.log"; then
    status=built
  fi

  manifest="${target_dir}/docs/manifest.txt"
  build_log="${target_dir}/docs/build-logs/${core}.log"
  make_args=""
  make_jobs=""
  opt_patch=""
  opt_flags=""
  output=""
  if [ -f "${manifest}" ]; then
    status_manifest=$(manifest_value "${manifest}" "${core}" status || true)
    [ -z "${status_manifest}" ] || status="${status_manifest}"
    make_args=$(manifest_value "${manifest}" "${core}" make_args || true)
    make_jobs=$(manifest_value "${manifest}" "${core}" make_jobs || true)
    opt_patch=$(manifest_value "${manifest}" "${core}" makefile_opt_patch || true)
    opt_flags=$(manifest_value "${manifest}" "${core}" makefile_opt_flags || true)
    output=$(awk '/^  output=/ { sub(/^  output=/, ""); print; exit }' "${manifest}")
  fi

  cross_cc=$(has_pattern "${build_log}" '^arm-linux-gnueabihf-(gcc|g\+\+)')
  host_cc=$(has_pattern "${build_log}" '^(gcc|g\+\+)([[:space:]]|$)')
  residual_o2=$(has_pattern "${build_log}" '[[:space:]]-O2([[:space:]]|$)')
  ofast=$(has_pattern "${build_log}" '[[:space:]]-Ofast([[:space:]]|$)')
  lto=$(has_pattern "${build_log}" '[[:space:]]-flto')
  neon=$(has_pattern "${build_log}" 'neon|HAVE_NEON')
  cortex_a7=$(has_pattern "${build_log}" 'cortex-a7')
  job_retry=$(has_pattern "${candidate_dir}/build.stderr.log" 'retrying lower parallelism')

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${core}" "${label}" "${requested}" "${status}" "${make_args}" "${make_jobs}" \
    "${opt_patch}" "${opt_flags}" "${output}" "${cross_cc}" "${host_cc}" \
    "${residual_o2}" "${ofast}" "${lto}" "${neon}" "${cortex_a7}" "${job_retry}" \
    "${candidate_dir}" >>"${summary}"

  index=$((index + 1))
done

printf '[probe-libretro] summary=%s\n' "${summary}" >&2
