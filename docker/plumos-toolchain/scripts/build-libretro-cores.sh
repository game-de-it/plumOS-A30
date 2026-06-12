#!/usr/bin/env bash
set -u

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-libretro-cores"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/libretro-cores/src"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

CORE_INFO_REPO=${CORE_INFO_REPO:-https://github.com/libretro/libretro-core-info.git}
CORE_INFO_REF=${CORE_INFO_REF:-HEAD}
CORE_RECIPES=${CORE_RECIPES:-"${ROOT_DIR}/docker/plumos-toolchain/libretro-core-recipes.tsv"}
PLUMOS_CORE_FILTER=${PLUMOS_CORE_FILTER:-plumos}
FAIL_ON_CORE_ERROR=${FAIL_ON_CORE_ERROR:-0}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
CXX=${CXX:-${CROSS_PREFIX}g++}
AR=${AR:-${CROSS_PREFIX}ar}
RANLIB=${RANLIB:-${CROSS_PREFIX}ranlib}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

COMMON_CFLAGS=${COMMON_CFLAGS:-"-O2 -pipe -marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fomit-frame-pointer -ffast-math"}
COMMON_CXXFLAGS=${COMMON_CXXFLAGS:-"${COMMON_CFLAGS}"}
COMMON_LDFLAGS=${COMMON_LDFLAGS:-""}

MANIFEST=""
LOG_DIR=""
BUILT_COUNT=0
FAILED_COUNT=0
SKIPPED_COUNT=0
ACTIVE_JOBS=${JOBS}

msg() {
  printf '[libretro-cores] %s\n' "$*" >&2
}

append_manifest() {
  printf '%s\n' "$*" >>"${MANIFEST}"
}

copy_if_present() {
  local src=$1
  local dst=$2
  if [ -f "${src}" ] && [ ! -f "${dst}/$(basename "${src}")" ]; then
    cp "${src}" "${dst}/"
  fi
}

find_target_lib() {
  local name=$1
  local dir
  for dir in \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/arm-linux-gnueabihf/lib \
    /lib \
    /usr/lib; do
    if [ -f "${dir}/${name}" ]; then
      printf '%s/%s\n' "${dir}" "${name}"
      return 0
    fi
  done
  return 1
}

tool_path() {
  command -v "$1" 2>/dev/null || printf '%s\n' "$1"
}

copy_runtime_deps() {
  local binary=$1
  local lib_dir=$2
  local lib path needed
  mkdir -p "${lib_dir}"

  for lib in ld-linux-armhf.so.3 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 \
    librt.so.1 libgcc_s.so.1 libstdc++.so.6; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    fi
  done

  needed=$("${READELF}" -d "${binary}" 2>/dev/null | awk '/NEEDED/ {gsub(/\[|\]/, "", $5); print $5}')
  for lib in ${needed}; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    else
      msg "warning: could not locate runtime dependency ${lib}"
    fi
  done
}

core_selected() {
  local id=$1
  local class=$2
  local filter=",$PLUMOS_CORE_FILTER,"
  local alias
  case "$PLUMOS_CORE_FILTER" in
    all|ALL|onion|ONION) return 0 ;;
    plumos|PLUMOS|class-plumos|Class-plumOS)
      [ "$class" = "A" ] || [ "$class" = "B" ]
      return
      ;;
    class-a|Class-A|a|A)
      [ "$class" = "A" ]
      return
      ;;
    class-b|Class-B|b|B)
      [ "$class" = "B" ]
      return
      ;;
    class-ab|Class-AB|ab|AB)
      [ "$class" = "A" ] || [ "$class" = "B" ]
      return
      ;;
    class-o|Class-O|o|O|onion-extra|Onion-extra)
      [ "$class" = "O" ]
      return
      ;;
  esac
  case "$filter" in
    *,"$id",*) return 0 ;;
  esac
  for alias in $(core_aliases "${id}"); do
    case "$filter" in
      *,"$alias",*) return 0 ;;
    esac
  done
  return 1
}

core_aliases() {
  case "$1" in
    mednafen_lynx) printf '%s\n' beetle_lynx ;;
    mednafen_ngp) printf '%s\n' beetle_ngp ;;
    mednafen_pce_fast) printf '%s\n' beetle_pce_fast ;;
    mednafen_supergrafx) printf '%s\n' beetle_supergrafx ;;
    mednafen_vb) printf '%s\n' beetle_vb ;;
    mednafen_wswan) printf '%s\n' beetle_wswan ;;
  esac
}

clone_repo() {
  local name=$1
  local repo=$2
  local ref=$3
  local dst="${SRC_ROOT}/${name}"
  local log=$4

  rm -rf "${dst}"
  mkdir -p "$(dirname "${dst}")"
  msg "cloning ${name} from ${repo} (${ref})"
  if ! git clone --depth 1 --recursive "${repo}" "${dst}" >>"${log}" 2>&1; then
    return 1
  fi
  if [ "${ref}" != "HEAD" ]; then
    (
      cd "${dst}" &&
        git fetch --depth 1 origin "${ref}" &&
        git checkout --detach FETCH_HEAD &&
        git submodule update --init --recursive --depth 1
    ) >>"${log}" 2>&1 || return 1
  fi
  return 0
}

find_makefile() {
  local work=$1
  local configured=$2
  local candidate

  if [ -n "${configured}" ] && [ -f "${work}/${configured}" ]; then
    printf '%s\n' "${configured}"
    return 0
  fi

  for candidate in \
    Makefile.libretro \
    libretro/Makefile \
    src/libretro/Makefile \
    platforms/libretro/Makefile \
    platform/libretro/Makefile \
    Makefile; do
    if [ -f "${work}/${candidate}" ]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}

make_clean() {
  local work=$1
  local makefile=$2
  (
    cd "${work}" &&
      env CC="${CC}" CXX="${CXX}" AR="${AR}" RANLIB="${RANLIB}" \
        make -f "${makefile}" clean >/dev/null 2>&1
  ) || true
}

run_make_attempt() {
  local work=$1
  local makefile=$2
  local log=$3
  shift 3

  make_clean "${work}" "${makefile}"
  (
    cd "${work}" &&
      env \
        CC="${CC}" \
        CXX="${CXX}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        CFLAGS="${COMMON_CFLAGS}" \
        CXXFLAGS="${COMMON_CXXFLAGS}" \
        LDFLAGS="${COMMON_LDFLAGS}" \
        make -f "${makefile}" -j"${ACTIVE_JOBS}" "$@"
  ) >>"${log}" 2>&1
}

build_with_fallbacks() {
  local work=$1
  local makefile=$2
  local configured_args=$3
  local log=$4
  local -a args

  if [ -n "${configured_args}" ]; then
    read -r -a args <<<"${configured_args}"
    msg "make $(basename "${work}") with configured args: ${configured_args}"
    if run_make_attempt "${work}" "${makefile}" "${log}" "${args[@]}"; then
      printf '%s\n' "${configured_args}"
      return 0
    fi
  fi

  for fallback in \
    "platform=armv7-hardfloat-neon" \
    "platform=armv HAVE_NEON=1" \
    "platform=unix" \
    ""; do
    read -r -a args <<<"${fallback}"
    msg "make $(basename "${work}") with fallback args: ${fallback:-<none>}"
    if run_make_attempt "${work}" "${makefile}" "${log}" "${args[@]}"; then
      printf '%s\n' "${fallback}"
      return 0
    fi
  done
  return 1
}

run_cmake_build() {
  local src=$1
  local build_dir=$2
  local target=$3
  local build_type=$4
  local log=$5
  local cmake_cc cmake_cxx cmake_ar cmake_ranlib cmake_strip
  shift 5

  cmake_cc=$(tool_path "${CC}")
  cmake_cxx=$(tool_path "${CXX}")
  cmake_ar=$(tool_path "${AR}")
  cmake_ranlib=$(tool_path "${RANLIB}")
  cmake_strip=$(tool_path "${STRIP}")

  rm -rf "${build_dir}"
  (
    cmake -S "${src}" -B "${build_dir}" -G Ninja \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=arm \
      -DCMAKE_C_COMPILER="${cmake_cc}" \
      -DCMAKE_CXX_COMPILER="${cmake_cxx}" \
      -DCMAKE_AR="${cmake_ar}" \
      -DCMAKE_RANLIB="${cmake_ranlib}" \
      -DCMAKE_STRIP="${cmake_strip}" \
      -DCMAKE_C_FLAGS="${COMMON_CFLAGS}" \
      -DCMAKE_CXX_FLAGS="${COMMON_CXXFLAGS}" \
      -DCMAKE_SHARED_LINKER_FLAGS="${COMMON_LDFLAGS}" \
      -DCMAKE_BUILD_TYPE="${build_type}" \
      "$@" &&
      cmake --build "${build_dir}" --target "${target}" -j"${ACTIVE_JOBS}"
  ) >>"${log}" 2>&1
}

run_scummvm_build() {
  local src=$1
  local log=$2

  (
    cd "${src}/backends/platform/libretro" &&
      env \
        CC="${CC}" \
        CXX="${CXX}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        CFLAGS="${COMMON_CFLAGS}" \
        CXXFLAGS="${COMMON_CXXFLAGS}" \
        LDFLAGS="${COMMON_LDFLAGS}" \
        make clean >/dev/null 2>&1 || true
    env \
      CC="${CC}" \
      CXX="${CXX}" \
      AR="${AR}" \
      RANLIB="${RANLIB}" \
      CFLAGS="${COMMON_CFLAGS}" \
      CXXFLAGS="${COMMON_CXXFLAGS}" \
      LDFLAGS="${COMMON_LDFLAGS}" \
      make -j"${ACTIVE_JOBS}" \
        platform=armv7-hardfloat-neon \
        TOOLSET="${CROSS_PREFIX}" \
        LITE=1 \
        NO_WIP=1 \
        FORCE_OPENGLNONE=1 \
        USE_MT32EMU= \
        USE_VORBIS= \
        USE_THEORADEC= \
        USE_FLUIDSYNTH= \
        USE_FREETYPE2= \
        USE_MPEG2= \
        USE_IMGUI=
  ) >>"${log}" 2>&1
}

build_inih_for_easyrpg() {
  local src=$1
  local log=$2
  local inih_dir="${src}/lib/inih"

  rm -rf "${inih_dir}"
  mkdir -p "$(dirname "${inih_dir}")"
  git clone --depth 1 https://github.com/benhoyt/inih.git "${inih_dir}" >>"${log}" 2>&1 || return 1
  (
    cd "${inih_dir}" &&
      env CC="${CC}" CFLAGS="${COMMON_CFLAGS} -fPIC" "${CC}" -c ini.c -o ini.o &&
      "${AR}" rcs libinih.a ini.o &&
      "${RANLIB}" libinih.a
  ) >>"${log}" 2>&1
}

build_special_core() {
  local id=$1
  local src=$2
  local log=$3

  case "${id}" in
    mgba)
      ACTIVE_JOBS=1
      msg "cmake ${id}: minimal libretro build"
      run_cmake_build "${src}" "${src}/build-libretro" mgba_libretro Release "${log}" \
        -DBUILD_LIBRETRO=ON \
        -DBUILD_SDL=OFF \
        -DBUILD_QT=OFF \
        -DBUILD_GL=OFF \
        -DBUILD_GLES2=OFF \
        -DBUILD_PGO=OFF \
        -DBUILD_PERF=OFF \
        -DBUILD_TEST=OFF \
        -DUSE_FFMPEG=OFF \
        -DUSE_DISCORD_RPC=OFF \
        -DUSE_EDITLINE=OFF
      ;;
    tic80)
      ACTIVE_JOBS=1
      msg "cmake ${id}: libretro build"
      run_cmake_build "${src}" "${src}/build-libretro" tic80_libretro MinSizeRel "${log}" \
        -DBUILD_PLAYER=OFF \
        -DBUILD_SDL=OFF \
        -DBUILD_SDLGPU=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_LIBRETRO=ON
      ;;
    easyrpg)
      ACTIVE_JOBS=1
      msg "cmake ${id}: libretro build"
      build_inih_for_easyrpg "${src}" "${log}" || return 1
      run_cmake_build "${src}" "${src}/build-libretro" easyrpg_libretro Release "${log}" \
        -DPLAYER_TARGET_PLATFORM=libretro \
        -DPLAYER_BUILD_LIBLCF=ON \
        -DINIH_INCLUDE_DIR="${src}/lib/inih" \
        -DINIH_LIBRARY="${src}/lib/inih/libinih.a" \
        -DLIBLCF_WITH_ICU=OFF \
        -DLIBLCF_WITH_XML=OFF \
        -DLIBLCF_ENABLE_TOOLS=OFF \
        -DLIBLCF_ENABLE_TESTS=OFF \
        -DLIBLCF_ENABLE_BENCHMARKS=OFF \
        -DBUILD_SHARED_LIBS=ON \
        -DPLAYER_WITH_FREETYPE=OFF \
        -DPLAYER_WITH_HARFBUZZ=OFF \
        -DPLAYER_WITH_LHASA=OFF \
        -DPLAYER_WITH_MPG123=OFF \
        -DPLAYER_WITH_LIBSNDFILE=OFF \
        -DPLAYER_WITH_OGGVORBIS=OFF \
        -DPLAYER_WITH_OPUS=OFF \
        -DPLAYER_WITH_WILDMIDI=OFF \
        -DPLAYER_WITH_FLUIDSYNTH=OFF \
        -DPLAYER_WITH_FLUIDLITE=OFF \
        -DPLAYER_WITH_XMP=OFF \
        -DPLAYER_WITH_SPEEXDSP=OFF \
        -DPLAYER_WITH_SAMPLERATE=OFF
      ;;
    scummvm)
      ACTIVE_JOBS=1
      msg "make ${id}: libretro build"
      run_scummvm_build "${src}" "${log}"
      ;;
    *)
      return 99
      ;;
  esac
}

stage_core_outputs() {
  local id=$1
  local src=$2
  local outputs
  local so base info

  outputs=$(find "${src}" -type f -name '*_libretro.so' | sort)
  if [ -z "${outputs}" ]; then
    return 1
  fi

  while IFS= read -r so; do
    [ -n "${so}" ] || continue
    base=$(basename "${so}")
    cp "${so}" "${TARGET_DIR}/plumos/retroarch/cores/${base}"
    "${STRIP}" "${TARGET_DIR}/plumos/retroarch/cores/${base}" >/dev/null 2>&1 || true
    copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/cores/${base}" "${TARGET_DIR}/plumos/lib"

    info="${SRC_ROOT}/core-info/${base%.so}.info"
    if [ -f "${info}" ]; then
      cp "${info}" "${TARGET_DIR}/plumos/retroarch/info/"
      cp "${info}" "${TARGET_DIR}/plumos/retroarch/cores/"
    fi

    printf '  output=%s\n' "plumos/retroarch/cores/${base}" >>"${MANIFEST}"
    printf '  needed=\n' >>"${MANIFEST}"
    "${READELF}" -d "${TARGET_DIR}/plumos/retroarch/cores/${base}" 2>/dev/null |
      awk '/NEEDED/ {print "    " $0}' >>"${MANIFEST}" || true
  done <<<"${outputs}"
  return 0
}

stage_license_files() {
  local id=$1
  local work=$2
  local file
  for file in \
    COPYING COPYING.txt LICENSE LICENSE.txt LICENSE.md COPYING.md Copying README.md; do
    if [ -f "${work}/${file}" ]; then
      cp "${work}/${file}" "${TARGET_DIR}/docs/${id}-${file}" 2>/dev/null || true
      return 0
    fi
  done
  return 0
}

build_one_core() {
  local id=$1
  local class=$2
  local repo=$3
  local ref=$4
  local subdir=$5
  local makefile_hint=$6
  local make_args=$7
  local log="${LOG_DIR}/${id}.log"
  local src="${SRC_ROOT}/${id}"
  local work makefile commit actual_args

  ACTIVE_JOBS=${JOBS}
  case "${id}" in
    neocd|np2kai|picodrive|snes9x)
      ACTIVE_JOBS=1
      ;;
  esac

  if ! core_selected "${id}" "${class}"; then
    SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    return 0
  fi

  : >"${log}"
  append_manifest ""
  append_manifest "[${id}]"
  append_manifest "class=${class}"
  append_manifest "repo=${repo}"
  append_manifest "ref=${ref}"
  append_manifest "log=docs/build-logs/${id}.log"

  if ! clone_repo "${id}" "${repo}" "${ref}" "${log}"; then
    msg "FAILED clone ${id}"
    append_manifest "status=failed"
    append_manifest "reason=clone_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  commit=$(git -C "${src}" rev-parse HEAD 2>/dev/null || echo unknown)
  append_manifest "commit=${commit}"

  local special_status
  build_special_core "${id}" "${src}" "${log}"
  special_status=$?
  if [ "${special_status}" -eq 0 ]; then
    append_manifest "builder=special"
    append_manifest "make_args=cmake/configure"
    append_manifest "status=built"
    if stage_core_outputs "${id}" "${src}"; then
      stage_license_files "${id}" "${src}"
      BUILT_COUNT=$((BUILT_COUNT + 1))
      msg "built ${id}"
    else
      msg "FAILED ${id}: no *_libretro.so output"
      append_manifest "status=failed_after_build"
      append_manifest "reason=no_output"
      FAILED_COUNT=$((FAILED_COUNT + 1))
    fi
    return 0
  elif [ "${special_status}" -ne 99 ]; then
    msg "FAILED special build ${id}"
    append_manifest "builder=special"
    append_manifest "status=failed"
    append_manifest "reason=build_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  work="${src}"
  if [ -n "${subdir}" ]; then
    work="${src}/${subdir}"
  fi
  if [ ! -d "${work}" ]; then
    msg "FAILED ${id}: missing subdir ${subdir}"
    append_manifest "status=failed"
    append_manifest "reason=missing_subdir:${subdir}"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  if ! makefile=$(find_makefile "${work}" "${makefile_hint}"); then
    msg "FAILED ${id}: no libretro makefile"
    append_manifest "status=failed"
    append_manifest "reason=no_makefile"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  append_manifest "makefile=${subdir:+${subdir}/}${makefile}"
  if actual_args=$(build_with_fallbacks "${work}" "${makefile}" "${make_args}" "${log}"); then
    append_manifest "make_args=${actual_args}"
  else
    msg "FAILED build ${id}"
    append_manifest "status=failed"
    append_manifest "reason=build_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  append_manifest "status=built"
  if stage_core_outputs "${id}" "${src}"; then
    stage_license_files "${id}" "${src}"
    BUILT_COUNT=$((BUILT_COUNT + 1))
    msg "built ${id}"
  else
    msg "FAILED ${id}: no *_libretro.so output"
    append_manifest "status=failed_after_build"
    append_manifest "reason=no_output"
    FAILED_COUNT=$((FAILED_COUNT + 1))
  fi
}

core_table() {
  awk '
    /^[[:space:]]*($|#)/ { next }
    { print }
  ' "${CORE_RECIPES}"
}

prepare_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p \
    "${TARGET_DIR}/plumos/lib" \
    "${TARGET_DIR}/plumos/retroarch/cores" \
    "${TARGET_DIR}/plumos/retroarch/info" \
    "${TARGET_DIR}/docs/build-logs"
  LOG_DIR="${TARGET_DIR}/docs/build-logs"
  MANIFEST="${TARGET_DIR}/docs/manifest.txt"
  {
    printf 'plumOS libretro core build\n'
    printf 'target=armv7-a cortex-a7 hard-float neon-vfpv4\n'
    printf 'core_recipes=%s\n' "${CORE_RECIPES}"
    printf 'common_cflags=%s\n' "${COMMON_CFLAGS}"
    printf 'core_filter=%s\n' "${PLUMOS_CORE_FILTER}"
  } >"${MANIFEST}"
}

clone_core_info() {
  local log="${LOG_DIR}/core-info.log"
  : >"${log}"
  if clone_repo core-info "${CORE_INFO_REPO}" "${CORE_INFO_REF}" "${log}"; then
    local commit
    commit=$(git -C "${SRC_ROOT}/core-info" rev-parse HEAD 2>/dev/null || echo unknown)
    append_manifest ""
    append_manifest "[core-info]"
    append_manifest "repo=${CORE_INFO_REPO}"
    append_manifest "ref=${CORE_INFO_REF}"
    append_manifest "commit=${commit}"
    append_manifest "log=docs/build-logs/core-info.log"
    return 0
  fi
  append_manifest ""
  append_manifest "[core-info]"
  append_manifest "repo=${CORE_INFO_REPO}"
  append_manifest "ref=${CORE_INFO_REF}"
  append_manifest "status=failed"
  append_manifest "reason=clone_failed"
  append_manifest "log=docs/build-logs/core-info.log"
  return 1
}

main() {
  local line id class repo ref subdir makefile make_args

  if [ ! -f "${CORE_RECIPES}" ]; then
    msg "error: core recipe file not found: ${CORE_RECIPES}"
    exit 1
  fi

  mkdir -p "${SRC_ROOT}"
  prepare_dist
  clone_core_info || true

  while IFS='|' read -r id class repo ref subdir makefile make_args; do
    [ -n "${id}" ] || continue
    build_one_core "${id}" "${class}" "${repo}" "${ref}" "${subdir}" "${makefile}" "${make_args}"
  done < <(core_table)

  {
    printf '\n[summary]\n'
    printf 'built=%d\n' "${BUILT_COUNT}"
    printf 'failed=%d\n' "${FAILED_COUNT}"
    printf 'skipped=%d\n' "${SKIPPED_COUNT}"
  } >>"${MANIFEST}"

  msg "staged ${TARGET_DIR}"
  msg "summary: built=${BUILT_COUNT} failed=${FAILED_COUNT} skipped=${SKIPPED_COUNT}"
  if [ "${FAIL_ON_CORE_ERROR}" = "1" ] && [ "${FAILED_COUNT}" -gt 0 ]; then
    exit 1
  fi
}

main "$@"
