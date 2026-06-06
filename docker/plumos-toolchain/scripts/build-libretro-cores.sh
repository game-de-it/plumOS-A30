#!/bin/sh
set -eu

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-libretro-cores"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/libretro-cores/src"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

FCEUMM_REPO=${FCEUMM_REPO:-https://github.com/libretro/libretro-fceumm.git}
FCEUMM_REF=${FCEUMM_REF:-HEAD}
GAMBATTE_REPO=${GAMBATTE_REPO:-https://github.com/libretro/gambatte-libretro.git}
GAMBATTE_REF=${GAMBATTE_REF:-HEAD}
CORE_INFO_REPO=${CORE_INFO_REPO:-https://github.com/libretro/libretro-core-info.git}
CORE_INFO_REF=${CORE_INFO_REF:-HEAD}

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

msg() {
  printf '[libretro-cores] %s\n' "$*"
}

clone_repo() {
  name=$1
  repo=$2
  ref=$3
  dst="${SRC_ROOT}/${name}"

  rm -rf "${dst}"
  mkdir -p "$(dirname "${dst}")"
  msg "cloning ${name} from ${repo} (${ref})"
  git clone --depth 1 "${repo}" "${dst}"
  if [ "${ref}" != "HEAD" ]; then
    cd "${dst}"
    git fetch --depth 1 origin "${ref}"
    git checkout --detach FETCH_HEAD
  fi
}

copy_if_present() {
  src=$1
  dst=$2
  if [ -f "${src}" ] && [ ! -f "${dst}/$(basename "${src}")" ]; then
    cp "${src}" "${dst}/"
  fi
}

find_target_lib() {
  name=$1
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

copy_runtime_deps() {
  binary=$1
  lib_dir=$2
  mkdir -p "${lib_dir}"

  for lib in ld-linux-armhf.so.3 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libgcc_s.so.1 libstdc++.so.6; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    fi
  done

  needed=$("${READELF}" -d "${binary}" | awk '/NEEDED/ {gsub(/\[|\]/, "", $5); print $5}')
  for lib in ${needed}; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    else
      msg "warning: could not locate runtime dependency ${lib}"
    fi
  done
}

build_fceumm() {
  cd "${SRC_ROOT}/fceumm"
  msg "building fceumm"
  make -f Makefile.libretro clean >/dev/null 2>&1 || true
  env \
    CC="${CC}" \
    CXX="${CXX}" \
    AR="${AR}" \
    RANLIB="${RANLIB}" \
    CFLAGS="${COMMON_CFLAGS}" \
    CXXFLAGS="${COMMON_CXXFLAGS}" \
    LDFLAGS="${COMMON_LDFLAGS}" \
    make -f Makefile.libretro -j"${JOBS}" \
      platform=armv7-hardfloat-neon \
      HAVE_NEON=1 \
      ARCH=arm \
      BUILTIN_GPU=neon
}

build_gambatte() {
  cd "${SRC_ROOT}/gambatte"
  msg "building gambatte"
  make -f Makefile.libretro clean >/dev/null 2>&1 || true
  env \
    CC="${CC}" \
    CXX="${CXX}" \
    AR="${AR}" \
    RANLIB="${RANLIB}" \
    CFLAGS="${COMMON_CFLAGS}" \
    CXXFLAGS="${COMMON_CXXFLAGS}" \
    LDFLAGS="${COMMON_LDFLAGS}" \
    make -f Makefile.libretro -j"${JOBS}" \
      platform=armv7-hardfloat-neon
}

stage_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p \
    "${TARGET_DIR}/plumos/lib" \
    "${TARGET_DIR}/plumos/retroarch/cores" \
    "${TARGET_DIR}/plumos/retroarch/info" \
    "${TARGET_DIR}/docs"

  cp "${SRC_ROOT}/fceumm/fceumm_libretro.so" "${TARGET_DIR}/plumos/retroarch/cores/"
  cp "${SRC_ROOT}/gambatte/gambatte_libretro.so" "${TARGET_DIR}/plumos/retroarch/cores/"
  "${STRIP}" "${TARGET_DIR}/plumos/retroarch/cores/fceumm_libretro.so" || true
  "${STRIP}" "${TARGET_DIR}/plumos/retroarch/cores/gambatte_libretro.so" || true

  cp "${SRC_ROOT}/core-info/fceumm_libretro.info" "${TARGET_DIR}/plumos/retroarch/info/"
  cp "${SRC_ROOT}/core-info/gambatte_libretro.info" "${TARGET_DIR}/plumos/retroarch/info/"
  cp "${SRC_ROOT}/core-info/fceumm_libretro.info" "${TARGET_DIR}/plumos/retroarch/cores/"
  cp "${SRC_ROOT}/core-info/gambatte_libretro.info" "${TARGET_DIR}/plumos/retroarch/cores/"

  cp "${SRC_ROOT}/fceumm/Copying" "${TARGET_DIR}/docs/fceumm-Copying"
  cp "${SRC_ROOT}/gambatte/COPYING" "${TARGET_DIR}/docs/gambatte-COPYING"

  copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/cores/fceumm_libretro.so" "${TARGET_DIR}/plumos/lib"
  copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/cores/gambatte_libretro.so" "${TARGET_DIR}/plumos/lib"

  fceumm_commit=$(git -C "${SRC_ROOT}/fceumm" rev-parse HEAD)
  gambatte_commit=$(git -C "${SRC_ROOT}/gambatte" rev-parse HEAD)
  core_info_commit=$(git -C "${SRC_ROOT}/core-info" rev-parse HEAD)

  {
    printf 'plumOS libretro core build\n'
    printf 'target=armv7-a cortex-a7 hard-float neon-vfpv4\n'
    printf 'common_cflags=%s\n' "${COMMON_CFLAGS}"
    printf '\n[fceumm]\n'
    printf 'repo=%s\n' "${FCEUMM_REPO}"
    printf 'ref=%s\n' "${FCEUMM_REF}"
    printf 'commit=%s\n' "${fceumm_commit}"
    printf 'make=platform=armv7-hardfloat-neon HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon\n'
    printf 'output=plumos/retroarch/cores/fceumm_libretro.so\n'
    printf 'needed=\n'
    "${READELF}" -d "${TARGET_DIR}/plumos/retroarch/cores/fceumm_libretro.so" | awk '/NEEDED/ {print "  " $0}'
    printf '\n[gambatte]\n'
    printf 'repo=%s\n' "${GAMBATTE_REPO}"
    printf 'ref=%s\n' "${GAMBATTE_REF}"
    printf 'commit=%s\n' "${gambatte_commit}"
    printf 'make=platform=armv7-hardfloat-neon VIDEO_RGB565=1\n'
    printf 'output=plumos/retroarch/cores/gambatte_libretro.so\n'
    printf 'needed=\n'
    "${READELF}" -d "${TARGET_DIR}/plumos/retroarch/cores/gambatte_libretro.so" | awk '/NEEDED/ {print "  " $0}'
    printf '\n[core-info]\n'
    printf 'repo=%s\n' "${CORE_INFO_REPO}"
    printf 'ref=%s\n' "${CORE_INFO_REF}"
    printf 'commit=%s\n' "${core_info_commit}"
  } > "${TARGET_DIR}/docs/manifest.txt"

  msg "staged ${TARGET_DIR}"
}

clone_repo fceumm "${FCEUMM_REPO}" "${FCEUMM_REF}"
clone_repo gambatte "${GAMBATTE_REPO}" "${GAMBATTE_REF}"
clone_repo core-info "${CORE_INFO_REPO}" "${CORE_INFO_REF}"
build_fceumm
build_gambatte
stage_dist
