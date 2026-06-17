#!/usr/bin/env bash
set -u

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-libretro-cores"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/libretro-cores/src"}
HOST_JOBS=${HOST_JOBS:-$(nproc 2>/dev/null || echo 2)}
LIBRETRO_CORE_BUILD_CONCURRENCY=${LIBRETRO_CORE_BUILD_CONCURRENCY:-4}

case "${HOST_JOBS}" in
  ''|*[!0-9]*) HOST_JOBS=2 ;;
esac
case "${LIBRETRO_CORE_BUILD_CONCURRENCY}" in
  ''|*[!0-9]*) LIBRETRO_CORE_BUILD_CONCURRENCY=4 ;;
esac
[ "${HOST_JOBS}" -gt 0 ] || HOST_JOBS=2
[ "${LIBRETRO_CORE_BUILD_CONCURRENCY}" -gt 0 ] || LIBRETRO_CORE_BUILD_CONCURRENCY=1

if [ -z "${JOBS:-}" ]; then
  JOBS=$(((HOST_JOBS + LIBRETRO_CORE_BUILD_CONCURRENCY - 1) / LIBRETRO_CORE_BUILD_CONCURRENCY))
fi
case "${JOBS}" in
  ''|*[!0-9]*) JOBS=1 ;;
esac
[ "${JOBS}" -gt 0 ] || JOBS=1

CORE_INFO_REPO=${CORE_INFO_REPO:-https://github.com/libretro/libretro-core-info.git}
CORE_INFO_REF=${CORE_INFO_REF:-HEAD}
CORE_RECIPES=${CORE_RECIPES:-"${ROOT_DIR}/docker/plumos-toolchain/libretro-core-recipes.tsv"}
PLUMOS_CORE_FILTER=${PLUMOS_CORE_FILTER:-plumos}
FAIL_ON_CORE_ERROR=${FAIL_ON_CORE_ERROR:-0}
BUILD_JOB_FALLBACKS=${BUILD_JOB_FALLBACKS:-1}
LIBRETRO_SERIAL_CORES=${LIBRETRO_SERIAL_CORES:-"nestopia quicknes gambatte gpsp picodrive mednafen_pce_fast mednafen_supergrafx mednafen_ngp mednafen_lynx handy prosystem gw pokemini mednafen_vb dinothawr mrboom tgbdual"}
LIBRETRO_OPTIMIZATION_PROFILE=${LIBRETRO_OPTIMIZATION_PROFILE:-speed}
LIBRETRO_ENABLE_LTO=${LIBRETRO_ENABLE_LTO:-0}
LIBRETRO_EXTRA_CFLAGS=${LIBRETRO_EXTRA_CFLAGS:-}
LIBRETRO_EXTRA_CXXFLAGS=${LIBRETRO_EXTRA_CXXFLAGS:-}
LIBRETRO_EXTRA_LDFLAGS=${LIBRETRO_EXTRA_LDFLAGS:-}
LIBRETRO_FORCE_MAKE_TOOLCHAIN=${LIBRETRO_FORCE_MAKE_TOOLCHAIN:-1}
LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS=${LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS:-1}
LIBRETRO_MAKE_OPT_FLAGS=${LIBRETRO_MAKE_OPT_FLAGS:-}
LIBRETRO_MAKE_ARGS_OVERRIDE=${LIBRETRO_MAKE_ARGS_OVERRIDE:-}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
CXX=${CXX:-${CROSS_PREFIX}g++}
AR=${AR:-${CROSS_PREFIX}ar}
RANLIB=${RANLIB:-${CROSS_PREFIX}ranlib}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

MANIFEST=""
LOG_DIR=""
BUILT_COUNT=0
FAILED_COUNT=0
SKIPPED_COUNT=0
ACTIVE_JOBS=${JOBS}
LAST_SUCCESSFUL_JOBS=""
LAST_SUCCESSFUL_ARGS=""

msg() {
  printf '[libretro-cores] %s\n' "$*" >&2
}

append_manifest() {
  printf '%s\n' "$*" >>"${MANIFEST}"
}

profile_cflags() {
  local cpu_flags="-pipe -marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
  local flags

  case "${LIBRETRO_OPTIMIZATION_PROFILE}" in
    compat)
      flags="-O2 ${cpu_flags} -DNDEBUG -fomit-frame-pointer"
      ;;
    speed)
      flags="-O3 ${cpu_flags} -DNDEBUG -fomit-frame-pointer -fno-semantic-interposition -fno-math-errno -fno-trapping-math"
      ;;
    aggressive|ofast)
      flags="-Ofast ${cpu_flags} -DNDEBUG -fomit-frame-pointer -fno-semantic-interposition -funroll-loops"
      ;;
    size)
      flags="-Os ${cpu_flags} -DNDEBUG -fomit-frame-pointer"
      ;;
    debug)
      flags="-Og -g ${cpu_flags}"
      ;;
    custom)
      flags="-O3 ${cpu_flags} -DNDEBUG -fomit-frame-pointer"
      ;;
    *)
      msg "error: unknown LIBRETRO_OPTIMIZATION_PROFILE=${LIBRETRO_OPTIMIZATION_PROFILE}"
      return 1
      ;;
  esac

  if [ "${LIBRETRO_ENABLE_LTO}" = "1" ]; then
    flags="${flags} -flto=auto"
  fi
  if [ -n "${LIBRETRO_EXTRA_CFLAGS}" ]; then
    flags="${flags} ${LIBRETRO_EXTRA_CFLAGS}"
  fi
  printf '%s\n' "${flags}"
}

profile_ldflags() {
  local flags=""
  if [ "${LIBRETRO_ENABLE_LTO}" = "1" ]; then
    flags="${flags} -flto=auto"
  fi
  if [ -n "${LIBRETRO_EXTRA_LDFLAGS}" ]; then
    flags="${flags} ${LIBRETRO_EXTRA_LDFLAGS}"
  fi
  printf '%s\n' "${flags# }"
}

profile_make_opt_flags() {
  local make_args=${1:-}

  case "${LIBRETRO_OPTIMIZATION_PROFILE}" in
    compat)
      printf '%s\n' "-O2"
      ;;
    speed)
      case " ${make_args} " in
        *" platform=classic_"*) printf '%s\n' "-Ofast" ;;
        *) printf '%s\n' "-O3" ;;
      esac
      ;;
    aggressive|ofast)
      printf '%s\n' "-Ofast"
      ;;
    size)
      printf '%s\n' "-Os"
      ;;
    debug)
      printf '%s\n' "-Og -g"
      ;;
    custom)
      printf '%s\n' "-O3"
      ;;
    *)
      msg "error: unknown LIBRETRO_OPTIMIZATION_PROFILE=${LIBRETRO_OPTIMIZATION_PROFILE}"
      return 1
      ;;
  esac
}

COMMON_CFLAGS=${COMMON_CFLAGS:-$(profile_cflags)}
COMMON_CXXFLAGS=${COMMON_CXXFLAGS:-"${COMMON_CFLAGS}${LIBRETRO_EXTRA_CXXFLAGS:+ ${LIBRETRO_EXTRA_CXXFLAGS}}"}
COMMON_LDFLAGS=${COMMON_LDFLAGS:-$(profile_ldflags)}

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

core_initial_jobs() {
  local id=$1
  local serial_cores=" ${LIBRETRO_SERIAL_CORES//,/ } "

  case "${serial_cores}" in
    *" ${id} "*) printf '%s\n' 1 ;;
    *) printf '%s\n' "${JOBS}" ;;
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
        case "${name}" in
          ecwolf)
            git submodule update --init --depth 1 src/libretro/libretro-common
            ;;
          *)
            git submodule update --init --recursive --depth 1
            ;;
        esac
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
  local -a toolchain_args=()

  if [ "${LIBRETRO_FORCE_MAKE_TOOLCHAIN}" = "1" ]; then
    toolchain_args=(CC="${CC}" CXX="${CXX}" AR="${AR}" RANLIB="${RANLIB}")
  fi

  (
    cd "${work}" &&
      env CC="${CC}" CXX="${CXX}" AR="${AR}" RANLIB="${RANLIB}" \
        make -f "${makefile}" clean "${toolchain_args[@]}" >/dev/null 2>&1
  ) || true
}

run_make_attempt() {
  local work=$1
  local makefile=$2
  local log=$3
  local -a toolchain_args=()
  shift 3

  if [ "${LIBRETRO_FORCE_MAKE_TOOLCHAIN}" = "1" ]; then
    toolchain_args=(CC="${CC}" CXX="${CXX}" AR="${AR}" RANLIB="${RANLIB}")
  fi

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
        make -f "${makefile}" -j"${ACTIVE_JOBS}" "$@" "${toolchain_args[@]}"
  ) >>"${log}" 2>&1
}

patch_makefile_optimization() {
  local work=$1
  local makefile=$2
  local log=$3
  local make_args=${4:-}
  local file="${work}/${makefile}"
  local opt_flags escaped_flags candidate rel
  local patched=0

  if [ "${LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS}" != "1" ]; then
    printf '%s\n' "makefile_opt_patch=disabled"
    return 0
  fi
  if [ ! -f "${file}" ]; then
    printf '%s\n' "makefile_opt_patch=not_needed"
    return 0
  fi
  opt_flags=${LIBRETRO_MAKE_OPT_FLAGS:-$(profile_make_opt_flags "${make_args}")}
  if [ -z "${opt_flags}" ]; then
    printf '%s\n' "makefile_opt_patch=not_needed"
    return 0
  fi

  escaped_flags=$(printf '%s' "${opt_flags}" | sed 's/[&@\\]/\\&/g')
  while IFS= read -r candidate; do
    if ! grep -Eq -- '(^|[[:space:]=])-O(1|2|3|s|fast)([[:space:]\\]|$)' "${candidate}"; then
      continue
    fi
    cp "${candidate}" "${candidate}.plumos-opt.bak"
    if ! sed -i -E "s@(^|[[:space:]=])-O(1|2|3|s|fast)([[:space:]\\]|$)@\\1${escaped_flags}\\3@g" "${candidate}"; then
      mv "${candidate}.plumos-opt.bak" "${candidate}"
      printf '%s\n' "makefile_opt_patch=failed"
      return 1
    fi
    rel=${candidate#"${work}/"}
    printf '\n[plumOS] patched makefile optimization flags in %s: %s\n' "${rel}" "${opt_flags}" >>"${log}"
    patched=$((patched + 1))
  done <<EOF_PATCH_FILES
$(find "${work}" -maxdepth 5 -type f \( \
  -name 'Makefile' -o \
  -name 'Makefile.*' -o \
  -name 'makefile' -o \
  -name 'makefile.*' -o \
  -name '*.mk' -o \
  -name '*.mak' \
\) ! -name '*.plumos-opt.bak' -print)
EOF_PATCH_FILES

  if [ "${patched}" -eq 0 ]; then
    printf '%s\n' "makefile_opt_patch=not_needed"
    return 0
  fi
  printf '%s\n' "makefile_opt_patch=applied"
  printf '%s\n' "makefile_opt_flags=${opt_flags}"
}

patch_core_source() {
  local id=$1
  local src=$2
  local log=$3
  local lua_makefile

  case "${id}" in
    mgba)
      if [ -f "${src}/src/core/CMakeLists.txt" ]; then
        sed -i -E '/^[[:space:]]+scripting\.c$/d' "${src}/src/core/CMakeLists.txt"
        printf '\n[plumOS] patched mgba minimal libretro build to omit core scripting.c\n' >>"${log}"
      fi
      if [ -f "${src}/CMakeLists.txt" ]; then
        sed -i -E \
          -e '/list\(APPEND UTIL_SRC \$\{CMAKE_CURRENT_BINARY_DIR\}\/version\.c\)/d' \
          -e 's|add_library\(\$\{BINARY_NAME\}_libretro SHARED \$\{CORE_SRC\} \$\{RETRO_SRC\} \$\{CORE_VFS_SRC\}\)|add_library(${BINARY_NAME}_libretro SHARED ${CORE_SRC} ${RETRO_SRC} ${CORE_VFS_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src/util/vfs/vfs-fd.c)|' \
          -e '/file\(GLOB OS_SRC \$\{CMAKE_CURRENT_SOURCE_DIR\}\/src\/platform\/posix\/\*\.c\)/a\
	list(REMOVE_ITEM OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/posix/memory.c)' \
          "${src}/CMakeLists.txt"
        printf '[plumOS] patched mgba libretro CMake source list for duplicate/missing VFS symbols\n' >>"${log}"
      fi
      ;;
    quasi88)
      if [ -f "${ROOT_DIR}/docker/plumos-toolchain/patches/quasi88-libretro-monitor-options.patch" ]; then
        patch -d "${src}" -p1 < "${ROOT_DIR}/docker/plumos-toolchain/patches/quasi88-libretro-monitor-options.patch"
        printf '\n[plumOS] patched quasi88 libretro monitor options\n' >>"${log}"
      fi
      ;;
    nekop2)
      if [ -f "${ROOT_DIR}/docker/plumos-toolchain/patches/nekop2-libretro-joypad-keyboard.patch" ]; then
        patch -d "${src}" -p1 < "${ROOT_DIR}/docker/plumos-toolchain/patches/nekop2-libretro-joypad-keyboard.patch"
        printf '\n[plumOS] patched nekop2 joypad-to-keyboard mapping\n' >>"${log}"
      fi
      ;;
    hatari)
      if [ -f "${ROOT_DIR}/docker/plumos-toolchain/patches/hatari-libretro-skip-empty-media-options.patch" ]; then
        if patch --dry-run -d "${src}" -p1 < "${ROOT_DIR}/docker/plumos-toolchain/patches/hatari-libretro-skip-empty-media-options.patch" >/dev/null 2>>"${log}"; then
          patch -d "${src}" -p1 < "${ROOT_DIR}/docker/plumos-toolchain/patches/hatari-libretro-skip-empty-media-options.patch" >>"${log}" 2>&1
          printf '\n[plumOS] patched hatari to skip empty media command-line options\n' >>"${log}"
        else
          printf '\n[plumOS] skipped hatari empty-media patch: source layout does not match\n' >>"${log}"
        fi
      fi
      ;;
    lutro)
      lua_makefile="${src}/deps/lua/src/Makefile"
      if [ -f "${lua_makefile}" ]; then
        sed -i -E \
          -e 's/^AR=[[:space:]]*ar rcu/AR= ar/' \
          -e 's/^\t\$\(AR\)[[:space:]]+\$@/\t$(AR) rcu $@/' \
          "${lua_makefile}"
        printf '\n[plumOS] patched lutro Lua Makefile for command-line AR override\n' >>"${log}"
      fi
      ;;
  esac
}

run_make_attempt_with_job_retry() {
  local work=$1
  local makefile=$2
  local log=$3
  local saved_jobs="${ACTIVE_JOBS}"
  local job seen=" "
  shift 3

  for job in ${saved_jobs} ${BUILD_JOB_FALLBACKS}; do
    [ -n "${job}" ] || continue
    case "${seen}" in
      *" ${job} "*) continue ;;
    esac
    seen="${seen}${job} "
    ACTIVE_JOBS="${job}"
    printf '\n[plumOS] make jobs=%s args=%s\n' "${ACTIVE_JOBS}" "$*" >>"${log}"
    if run_make_attempt "${work}" "${makefile}" "${log}" "$@"; then
      LAST_SUCCESSFUL_JOBS="${ACTIVE_JOBS}"
      ACTIVE_JOBS="${saved_jobs}"
      return 0
    fi
    if [ "${ACTIVE_JOBS}" != "1" ]; then
      msg "make $(basename "${work}") failed with jobs=${ACTIVE_JOBS}; retrying lower parallelism"
    fi
  done

  ACTIVE_JOBS="${saved_jobs}"
  return 1
}

build_configured_recipe() {
  local work=$1
  local makefile=$2
  local configured_args=$3
  local log=$4
  local -a args

  if [ -n "${configured_args}" ]; then
    read -r -a args <<<"${configured_args}"
    msg "make $(basename "${work}") with configured args: ${configured_args}"
    if run_make_attempt_with_job_retry "${work}" "${makefile}" "${log}" "${args[@]}"; then
      LAST_SUCCESSFUL_ARGS="${configured_args}"
      return 0
    fi
    return 1
  fi

  msg "make $(basename "${work}") with recipe default args"
  if run_make_attempt_with_job_retry "${work}" "${makefile}" "${log}"; then
    LAST_SUCCESSFUL_ARGS=""
    return 0
  fi
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

run_cmake_build_with_job_retry() {
  local src=$1
  local build_dir=$2
  local target=$3
  local build_type=$4
  local log=$5
  local saved_jobs="${ACTIVE_JOBS}"
  local job seen=" "
  shift 5

  for job in ${saved_jobs} ${BUILD_JOB_FALLBACKS}; do
    [ -n "${job}" ] || continue
    case "${seen}" in
      *" ${job} "*) continue ;;
    esac
    seen="${seen}${job} "
    ACTIVE_JOBS="${job}"
    msg "cmake $(basename "${src}"): build ${target} with jobs=${ACTIVE_JOBS}"
    printf '\n[plumOS] cmake jobs=%s target=%s\n' "${ACTIVE_JOBS}" "${target}" >>"${log}"
    if run_cmake_build "${src}" "${build_dir}" "${target}" "${build_type}" "${log}" "$@"; then
      LAST_SUCCESSFUL_JOBS="${ACTIVE_JOBS}"
      ACTIVE_JOBS="${saved_jobs}"
      return 0
    fi
    if [ "${ACTIVE_JOBS}" != "1" ]; then
      msg "cmake $(basename "${src}") failed with jobs=${ACTIVE_JOBS}; retrying lower parallelism"
    fi
  done

  ACTIVE_JOBS="${saved_jobs}"
  return 1
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

run_scummvm_build_with_job_retry() {
  local src=$1
  local log=$2
  local saved_jobs="${ACTIVE_JOBS}"
  local job seen=" "

  for job in ${saved_jobs} ${BUILD_JOB_FALLBACKS}; do
    [ -n "${job}" ] || continue
    case "${seen}" in
      *" ${job} "*) continue ;;
    esac
    seen="${seen}${job} "
    ACTIVE_JOBS="${job}"
    msg "make scummvm: libretro build with jobs=${ACTIVE_JOBS}"
    printf '\n[plumOS] scummvm jobs=%s\n' "${ACTIVE_JOBS}" >>"${log}"
    if run_scummvm_build "${src}" "${log}"; then
      LAST_SUCCESSFUL_JOBS="${ACTIVE_JOBS}"
      ACTIVE_JOBS="${saved_jobs}"
      return 0
    fi
    if [ "${ACTIVE_JOBS}" != "1" ]; then
      msg "make scummvm failed with jobs=${ACTIVE_JOBS}; retrying lower parallelism"
    fi
  done

  ACTIVE_JOBS="${saved_jobs}"
  return 1
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

prepare_liblcf_for_easyrpg() {
  local src=$1
  local log=$2
  local liblcf_dir="${src}/lib/liblcf"
  local liblcf_ref=${EASYRPG_LIBLCF_REF:-abc215345ba962a031f2b8c645f4357cf1bece85}

  rm -rf "${liblcf_dir}"
  mkdir -p "$(dirname "${liblcf_dir}")"
  git clone --depth 1 https://github.com/EasyRPG/liblcf.git "${liblcf_dir}" >>"${log}" 2>&1 || return 1
  (
    cd "${liblcf_dir}" &&
      git fetch --depth 1 origin "${liblcf_ref}" &&
      git checkout --detach FETCH_HEAD
  ) >>"${log}" 2>&1 || return 1
  printf '\n[plumOS] pinned EasyRPG liblcf to %s\n' "${liblcf_ref}" >>"${log}"
}

build_special_core() {
  local id=$1
  local src=$2
  local log=$3

  case "${id}" in
    mgba)
      msg "cmake ${id}: minimal libretro build"
      run_cmake_build_with_job_retry "${src}" "${src}/build-libretro" mgba_libretro Release "${log}" \
        -DBUILD_LIBRETRO=ON \
        -DBUILD_SDL=OFF \
        -DBUILD_QT=OFF \
        -DBUILD_GL=OFF \
        -DBUILD_GLES2=OFF \
        -DBUILD_PGO=OFF \
        -DBUILD_LTO=OFF \
        -DBUILD_PERF=OFF \
        -DBUILD_TEST=OFF \
        -DBUILD_DOCGEN=OFF \
        -DENABLE_SCRIPTING=OFF \
        -DUSE_LUA=OFF \
        -DUSE_JSON_C=OFF \
        -DUSE_FREETYPE=OFF \
        -DUSE_FFMPEG=OFF \
        -DUSE_DISCORD_RPC=OFF \
        -DUSE_EDITLINE=OFF
      ;;
    tic80)
      msg "cmake ${id}: libretro build"
      local tic80_src="${src}"
      if [ -f "${src}/core/CMakeLists.txt" ]; then
        tic80_src="${src}/core"
      fi
      run_cmake_build_with_job_retry "${tic80_src}" "${src}/build-libretro" tic80_libretro MinSizeRel "${log}" \
        -DBUILD_PLAYER=OFF \
        -DBUILD_SDL=OFF \
        -DBUILD_SDLGPU=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_LIBRETRO=ON \
        -DBUILD_WITH_MRUBY=OFF
      ;;
    easyrpg)
      msg "cmake ${id}: libretro build"
      prepare_liblcf_for_easyrpg "${src}" "${log}" || return 1
      build_inih_for_easyrpg "${src}" "${log}" || return 1
      run_cmake_build_with_job_retry "${src}" "${src}/build-libretro" easyrpg_libretro Release "${log}" \
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
      msg "make ${id}: libretro build"
      run_scummvm_build_with_job_retry "${src}" "${log}"
      ;;
    squirreljme)
      if [ ! -f "${src}/nanocoat/CMakeLists.txt" ]; then
        return 99
      fi
      msg "cmake ${id}: nanocoat libretro build"
      run_cmake_build_with_job_retry "${src}/nanocoat" "${src}/nanocoat/build-libretro" squirreljme_libretro Release "${log}" \
        -DSQUIRRELJME_ENABLE_FRONTEND_LIBRETRO=ON \
        -DSQUIRRELJME_ENABLE_FRONTEND_JRI=OFF \
        -DSQUIRRELJME_ENABLE_DYLIB=ON \
        -DSQUIRRELJME_ENABLE_PACKING=OFF \
        -DLIBRETRO_REALLY_STATIC=OFF
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
  local so source_base base info

  outputs=$(
    find "${src}" -type f -name '*_libretro.so' | sort
    find "${src}" -type f -name '*_libretro.dll' | sort
  )
  if [ -z "${outputs}" ]; then
    return 1
  fi

  while IFS= read -r so; do
    [ -n "${so}" ] || continue
    if ! "${READELF}" -h "${so}" >/dev/null 2>&1; then
      continue
    fi
    source_base=$(basename "${so}")
    base="${source_base}"
    case "${base}" in
      *_libretro.dll) base="${base%.dll}.so" ;;
    esac
    if [ -f "${TARGET_DIR}/plumos/retroarch/cores/${base}" ]; then
      continue
    fi
    cp "${so}" "${TARGET_DIR}/plumos/retroarch/cores/${base}"
    "${STRIP}" "${TARGET_DIR}/plumos/retroarch/cores/${base}" >/dev/null 2>&1 || true
    copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/cores/${base}" "${TARGET_DIR}/plumos/lib"

    info="${SRC_ROOT}/core-info/${base%.so}.info"
    if [ ! -f "${info}" ]; then
      info=$(find "${src}" -type f -name "${base%.so}.info" | sort | head -n 1)
    fi
    if [ -f "${info}" ]; then
      cp "${info}" "${TARGET_DIR}/plumos/retroarch/info/"
      cp "${info}" "${TARGET_DIR}/plumos/retroarch/cores/"
    fi

    printf '  output=%s\n' "plumos/retroarch/cores/${base}" >>"${MANIFEST}"
    if [ "${source_base}" != "${base}" ]; then
      printf '  source_output=%s\n' "${source_base}" >>"${MANIFEST}"
    fi
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
  local work makefile commit effective_make_args
  local opt_patch_status

  LAST_SUCCESSFUL_JOBS=""
  LAST_SUCCESSFUL_ARGS=""
  effective_make_args="${make_args}"

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
  if [ -n "${LIBRETRO_MAKE_ARGS_OVERRIDE}" ]; then
    append_manifest "recipe_make_args=${make_args}"
    append_manifest "make_args_override=${LIBRETRO_MAKE_ARGS_OVERRIDE}"
    effective_make_args="${LIBRETRO_MAKE_ARGS_OVERRIDE}"
  fi

  if ! clone_repo "${id}" "${repo}" "${ref}" "${log}"; then
    msg "FAILED clone ${id}"
    append_manifest "status=failed"
    append_manifest "reason=clone_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  commit=$(git -C "${src}" rev-parse HEAD 2>/dev/null || echo unknown)
  append_manifest "commit=${commit}"
  patch_core_source "${id}" "${src}" "${log}"

  local special_status
  build_special_core "${id}" "${src}" "${log}"
  special_status=$?
  if [ "${special_status}" -eq 0 ]; then
    append_manifest "builder=special"
    append_manifest "make_args=cmake/configure"
    append_manifest "make_jobs=${LAST_SUCCESSFUL_JOBS:-${ACTIVE_JOBS}}"
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
  if ! opt_patch_status=$(patch_makefile_optimization "${work}" "${makefile}" "${log}" "${effective_make_args}"); then
    append_manifest "${opt_patch_status}"
    msg "FAILED ${id}: makefile optimization patch failed"
    append_manifest "status=failed"
    append_manifest "reason=makefile_opt_patch_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi
  append_manifest "${opt_patch_status}"
  if build_configured_recipe "${work}" "${makefile}" "${effective_make_args}" "${log}"; then
    append_manifest "make_args=${LAST_SUCCESSFUL_ARGS}"
    append_manifest "make_jobs=${LAST_SUCCESSFUL_JOBS:-${ACTIVE_JOBS}}"
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
    printf 'optimization_profile=%s\n' "${LIBRETRO_OPTIMIZATION_PROFILE}"
    printf 'enable_lto=%s\n' "${LIBRETRO_ENABLE_LTO}"
    printf 'common_cflags=%s\n' "${COMMON_CFLAGS}"
    printf 'common_cxxflags=%s\n' "${COMMON_CXXFLAGS}"
    printf 'common_ldflags=%s\n' "${COMMON_LDFLAGS}"
    printf 'make_opt_flags=%s\n' "${LIBRETRO_MAKE_OPT_FLAGS:-auto}"
    printf 'patch_makefile_opt_flags=%s\n' "${LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS}"
    printf 'force_make_toolchain=%s\n' "${LIBRETRO_FORCE_MAKE_TOOLCHAIN}"
    printf 'make_platform_fallbacks=disabled\n'
    printf 'make_args_override=%s\n' "${LIBRETRO_MAKE_ARGS_OVERRIDE}"
    printf 'core_filter=%s\n' "${PLUMOS_CORE_FILTER}"
    printf 'host_jobs=%s\n' "${HOST_JOBS}"
    printf 'core_build_concurrency=%s\n' "${LIBRETRO_CORE_BUILD_CONCURRENCY}"
    printf 'jobs=%s\n' "${JOBS}"
    printf 'serial_cores=%s\n' "${LIBRETRO_SERIAL_CORES}"
    printf 'job_fallbacks=%s\n' "${BUILD_JOB_FALLBACKS}"
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

prepare_core_job_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p \
    "${TARGET_DIR}/plumos/lib" \
    "${TARGET_DIR}/plumos/retroarch/cores" \
    "${TARGET_DIR}/plumos/retroarch/info" \
    "${TARGET_DIR}/docs/build-logs"
  LOG_DIR="${TARGET_DIR}/docs/build-logs"
  MANIFEST="${TARGET_DIR}/docs/manifest.txt"
  : >"${MANIFEST}"
}

run_core_job() {
  local id=$1
  local class=$2
  local repo=$3
  local ref=$4
  local subdir=$5
  local makefile=$6
  local make_args=$7
  local job_target=$8
  local rc

  TARGET_DIR="${job_target}"
  BUILT_COUNT=0
  FAILED_COUNT=0
  SKIPPED_COUNT=0
  ACTIVE_JOBS=$(core_initial_jobs "${id}")
  LAST_SUCCESSFUL_JOBS=""
  LAST_SUCCESSFUL_ARGS=""

  prepare_core_job_dist
  build_one_core "${id}" "${class}" "${repo}" "${ref}" "${subdir}" "${makefile}" "${make_args}"
  rc=$?
  {
    printf 'built=%d\n' "${BUILT_COUNT}"
    printf 'failed=%d\n' "${FAILED_COUNT}"
    printf 'skipped=%d\n' "${SKIPPED_COUNT}"
    printf 'exit_code=%d\n' "${rc}"
  } >"${TARGET_DIR}/status.env"
  return "${rc}"
}

running_core_jobs() {
  jobs -pr | wc -l | tr -d '[:space:]'
}

wait_for_core_slot() {
  local running
  while :; do
    running=$(running_core_jobs)
    [ "${running:-0}" -lt "${LIBRETRO_CORE_BUILD_CONCURRENCY}" ] && return 0
    sleep 1
  done
}

wait_for_core_jobs() {
  wait || true
}

copy_dir_contents() {
  local src=$1
  local dst=$2
  local entry

  [ -d "${src}" ] || return 0
  mkdir -p "${dst}"
  while IFS= read -r entry; do
    [ -n "${entry}" ] || continue
    cp -a "${entry}" "${dst}/"
  done < <(find "${src}" -mindepth 1 -maxdepth 1 -print)
}

status_value() {
  local file=$1
  local key=$2
  awk -F= -v key="${key}" '$1 == key { print $2; found = 1; exit } END { if (!found) exit 1 }' "${file}"
}

append_missing_job_manifest() {
  local id=$1
  local class=$2
  local repo=$3
  local ref=$4
  local reason=$5

  append_manifest ""
  append_manifest "[${id}]"
  append_manifest "class=${class}"
  append_manifest "repo=${repo}"
  append_manifest "ref=${ref}"
  append_manifest "status=failed"
  append_manifest "reason=${reason}"
}

merge_core_job() {
  local id=$1
  local class=$2
  local repo=$3
  local ref=$4
  local job_target=$5
  local job_manifest="${job_target}/docs/manifest.txt"
  local status_file="${job_target}/status.env"
  local built=0
  local failed=1
  local skipped=0

  copy_dir_contents "${job_target}/plumos/lib" "${TARGET_DIR}/plumos/lib"
  copy_dir_contents "${job_target}/plumos/retroarch/cores" "${TARGET_DIR}/plumos/retroarch/cores"
  copy_dir_contents "${job_target}/plumos/retroarch/info" "${TARGET_DIR}/plumos/retroarch/info"
  copy_dir_contents "${job_target}/docs/build-logs" "${TARGET_DIR}/docs/build-logs"
  if [ -d "${job_target}/docs" ]; then
    find "${job_target}/docs" -mindepth 1 -maxdepth 1 -type f ! -name manifest.txt -exec cp -a {} "${TARGET_DIR}/docs/" \;
  fi

  if [ -f "${status_file}" ]; then
    built=$(status_value "${status_file}" built || printf '0')
    failed=$(status_value "${status_file}" failed || printf '0')
    skipped=$(status_value "${status_file}" skipped || printf '0')
  fi

  if [ -f "${job_manifest}" ]; then
    cat "${job_manifest}" >>"${MANIFEST}"
  else
    append_missing_job_manifest "${id}" "${class}" "${repo}" "${ref}" "job_manifest_missing"
    if [ -f "${status_file}" ]; then
      failed=$((failed + 1))
    fi
  fi

  BUILT_COUNT=$((BUILT_COUNT + built))
  FAILED_COUNT=$((FAILED_COUNT + failed))
  SKIPPED_COUNT=$((SKIPPED_COUNT + skipped))
}

main() {
  local line id class repo ref subdir makefile make_args
  local job_root="${TARGET_DIR}/.core-jobs"
  local job_target
  local -a selected_rows=()

  if [ ! -f "${CORE_RECIPES}" ]; then
    msg "error: core recipe file not found: ${CORE_RECIPES}"
    exit 1
  fi

  mkdir -p "${SRC_ROOT}"
  prepare_dist
  clone_core_info || true

  while IFS='|' read -r id class repo ref subdir makefile make_args; do
    [ -n "${id}" ] || continue
    if core_selected "${id}" "${class}"; then
      selected_rows+=("${id}|${class}|${repo}|${ref}|${subdir}|${makefile}|${make_args}")
    else
      SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    fi
  done < <(core_table)

  mkdir -p "${job_root}"
  msg "core build concurrency=${LIBRETRO_CORE_BUILD_CONCURRENCY}; per-core jobs=${JOBS}; job fallbacks=${BUILD_JOB_FALLBACKS}"
  msg "selected cores=${#selected_rows[@]} skipped=${SKIPPED_COUNT}"

  for line in "${selected_rows[@]}"; do
    IFS='|' read -r id class repo ref subdir makefile make_args <<<"${line}"
    job_target="${job_root}/${id}"
    wait_for_core_slot
    run_core_job "${id}" "${class}" "${repo}" "${ref}" "${subdir}" "${makefile}" "${make_args}" "${job_target}" &
  done

  wait_for_core_jobs

  for line in "${selected_rows[@]}"; do
    IFS='|' read -r id class repo ref subdir makefile make_args <<<"${line}"
    merge_core_job "${id}" "${class}" "${repo}" "${ref}" "${job_root}/${id}"
  done
  rm -rf "${job_root}"

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
