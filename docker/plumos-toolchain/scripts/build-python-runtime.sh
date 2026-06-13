#!/usr/bin/env bash
set -euo pipefail

PYTHON_VERSION="${PYTHON_VERSION:-3.14.6}"
PYTHON_MAJOR_MINOR="${PYTHON_MAJOR_MINOR:-${PYTHON_VERSION%.*}}"
PYTHON_SHA256="${PYTHON_SHA256:-143b1dddefaec3bd2e21e3b839b34a2b7fb9842272883c576420d605e9f30c63}"
PYTHON_URL="${PYTHON_URL:-https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz}"
PLUMOS_PYTHON_PREFIX="${PLUMOS_PYTHON_PREFIX:-/mnt/SDCARD/plumos/python}"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
DOWNLOAD_DIR="${ROOT_DIR}/build/downloads"
BUILD_ROOT="${ROOT_DIR}/build/python-runtime"
SRC_DIR="${BUILD_ROOT}/Python-${PYTHON_VERSION}"
HOST_BUILD_DIR="${BUILD_ROOT}/host-build"
TARGET_BUILD_DIR="${BUILD_ROOT}/target-build"
STAGE_DIR="${BUILD_ROOT}/stage"
DIST_DIR="${ROOT_DIR}/dist/plumos-python-runtime"
PLUMOS_DIR="${DIST_DIR}/plumos"
BIN_DIR="${PLUMOS_DIR}/bin"
LIB_DIR="${PLUMOS_DIR}/lib"
PYTHON_DIR="${PLUMOS_DIR}/python"
DOC_DIR="${PLUMOS_DIR}/share/doc/plumos-python-runtime"
MANIFEST="${DOC_DIR}/manifest.txt"
TARBALL="${DOWNLOAD_DIR}/Python-${PYTHON_VERSION}.tar.xz"

CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
CC="${CC:-${CROSS_COMPILE}gcc}"
CXX="${CXX:-${CROSS_COMPILE}g++}"
AR="${AR:-${CROSS_COMPILE}ar}"
RANLIB="${RANLIB:-${CROSS_COMPILE}ranlib}"
STRIP="${STRIP:-${CROSS_COMPILE}strip}"
READELF="${READELF:-${CROSS_COMPILE}readelf}"
TARGET_GNU_TYPE="${TARGET_GNU_TYPE:-arm-linux-gnueabihf}"
BUILD_GNU_TYPE="${BUILD_GNU_TYPE:-$(dpkg-architecture -qDEB_BUILD_GNU_TYPE 2>/dev/null || gcc -dumpmachine)}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

TARGET_CFLAGS="${TARGET_CFLAGS:--O2 -pipe -marm -march=${PLUMOS_MARCH:-armv7-a} -mtune=cortex-a7 -mfpu=${PLUMOS_MFPU:-neon-vfpv4} -mfloat-abi=${PLUMOS_MFLOAT_ABI:-hard} -fomit-frame-pointer}"
TARGET_CPPFLAGS="${TARGET_CPPFLAGS:--I/usr/include/arm-linux-gnueabihf}"
TARGET_LDFLAGS="${TARGET_LDFLAGS:--L/usr/lib/arm-linux-gnueabihf -L/lib/arm-linux-gnueabihf}"
PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig}"

COPIED_RUNTIME_LIBS=""

log() {
  printf '%s\n' "==> $*"
}

die() {
  printf '%s\n' "error: $*" >&2
  exit 1
}

sha256_of() {
  sha256sum "$1" | awk '{print $1}'
}

download_checked() {
  local url="$1"
  local output="$2"
  local expected="$3"
  local actual

  mkdir -p "$DOWNLOAD_DIR"
  if [ -f "$output" ] && [ "$(sha256_of "$output")" = "$expected" ]; then
    log "Using cached $(basename "$output")"
    return
  fi

  log "Downloading $(basename "$output")"
  curl -fsSL "$url" -o "$output"
  actual="$(sha256_of "$output")"
  [ "$actual" = "$expected" ] || die "$(basename "$output") SHA-256 mismatch: ${actual}"
}

prepare_source() {
  rm -rf "$SRC_DIR"
  mkdir -p "$SRC_DIR"
  tar -C "$SRC_DIR" --strip-components=1 -xf "$TARBALL"
}

find_runtime_lib() {
  local soname="$1"
  local dir

  for dir in \
    "$LIB_DIR" \
    "${PYTHON_DIR}/lib" \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/arm-linux-gnueabihf/lib \
    /lib \
    /usr/lib; do
    if [ -e "${dir}/${soname}" ]; then
      readlink -f "${dir}/${soname}"
      return 0
    fi
  done

  return 1
}

runtime_needed() {
  local elf="$1"
  "$READELF" -d "$elf" 2>/dev/null | awk '/NEEDED/ { gsub(/[][]/, "", $5); print $5 }'
}

host_build_python_name() {
  local buildexe

  buildexe="$(awk -F= '/^BUILDEXE=/ { gsub(/[ \t]/, "", $2); print $2; exit }' "${HOST_BUILD_DIR}/Makefile" 2>/dev/null || true)"
  printf 'python%s\n' "$buildexe"
}

copy_runtime_lib_tree() {
  local soname="$1"
  local lib

  case "$soname" in
    libEGL.so*|libGLESv2.so*|libMali.so*)
      return 0
      ;;
  esac

  case " ${COPIED_RUNTIME_LIBS} " in
    *" ${soname} "*) return 0 ;;
  esac

  lib="$(find_runtime_lib "$soname")" || {
    printf 'warning: runtime library not found: %s\n' "$soname" >&2
    return 0
  }

  if [ "$(dirname "$lib")" != "$LIB_DIR" ] && [ "$(dirname "$lib")" != "${PYTHON_DIR}/lib" ]; then
    install -m 0755 "$lib" "${LIB_DIR}/${soname}"
    lib="${LIB_DIR}/${soname}"
  fi

  COPIED_RUNTIME_LIBS="${COPIED_RUNTIME_LIBS} ${soname}"

  while IFS= read -r child; do
    [ -n "$child" ] || continue
    copy_runtime_lib_tree "$child"
  done < <(runtime_needed "$lib")
}

copy_runtime_deps() {
  local elf="$1"
  local soname

  while IFS= read -r soname; do
    [ -n "$soname" ] || continue
    copy_runtime_lib_tree "$soname"
  done < <(runtime_needed "$elf")
}

copy_loader() {
  local loader

  for loader in \
    /lib/ld-linux-armhf.so.3 \
    /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 \
    /usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3; do
    if [ -e "$loader" ]; then
      install -m 0755 "$(readlink -f "$loader")" "$LIB_DIR/ld-linux-armhf.so.3"
      return 0
    fi
  done
  die "armhf dynamic loader not found"
}

build_host_python() {
  local build_python

  log "Building host Python ${PYTHON_VERSION}"
  rm -rf "$HOST_BUILD_DIR"
  mkdir -p "$HOST_BUILD_DIR"
  (
    cd "$HOST_BUILD_DIR"
    CC=gcc \
    CXX=g++ \
    AR=ar \
    RANLIB=ranlib \
    STRIP=strip \
    CFLAGS= \
    CPPFLAGS= \
    LDFLAGS= \
    PKG_CONFIG_ALLOW_CROSS= \
    PKG_CONFIG_LIBDIR= \
      "$SRC_DIR/configure" \
      --prefix="${BUILD_ROOT}/host-install" \
      --with-ensurepip=no
    build_python="$(host_build_python_name)"
    make -j"$JOBS" "$build_python" Programs/_freeze_module
  )
}

configure_target_python() {
  local config_site="${BUILD_ROOT}/config.site-armhf"
  local build_python="${HOST_BUILD_DIR}/$(host_build_python_name)"

  [ -x "$build_python" ] || die "host build Python not found: ${build_python}"

  cat > "$config_site" <<'EOF'
ac_cv_buggy_getaddrinfo=no
ac_cv_file__dev_ptmx=yes
ac_cv_file__dev_ptc=no
EOF

  log "Configuring target Python ${PYTHON_VERSION}"
  rm -rf "$TARGET_BUILD_DIR"
  mkdir -p "$TARGET_BUILD_DIR"
  (
    cd "$TARGET_BUILD_DIR"
    CONFIG_SITE="$config_site" \
    CC="$CC" \
    CXX="$CXX" \
    AR="$AR" \
    RANLIB="$RANLIB" \
    CFLAGS="$TARGET_CFLAGS" \
    CPPFLAGS="$TARGET_CPPFLAGS" \
    LDFLAGS="$TARGET_LDFLAGS" \
    PKG_CONFIG_LIBDIR="$PKG_CONFIG_LIBDIR" \
      "$SRC_DIR/configure" \
        --build="$BUILD_GNU_TYPE" \
        --host="$TARGET_GNU_TYPE" \
        --prefix="$PLUMOS_PYTHON_PREFIX" \
        --enable-shared \
        --disable-test-modules \
        --with-build-python="$build_python" \
        --with-ensurepip=no
  )
}

build_target_python() {
  log "Building target Python ${PYTHON_VERSION}"
  make -C "$TARGET_BUILD_DIR" -j"$JOBS"
}

install_target_python() {
  log "Installing target Python ${PYTHON_VERSION}"
  rm -rf "$STAGE_DIR" "$DIST_DIR"
  mkdir -p "$STAGE_DIR" "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"
  make -C "$TARGET_BUILD_DIR" install DESTDIR="$STAGE_DIR"
  mkdir -p "$PYTHON_DIR"
  cp -a "${STAGE_DIR}${PLUMOS_PYTHON_PREFIX}/." "$PYTHON_DIR/"
}

materialize_symlinks() {
  local link
  local target

  log "Materializing symlinks for SD-card filesystems"
  while IFS= read -r link; do
    target="$(readlink -f "$link")" || die "broken symlink: ${link}"
    [ -f "$target" ] || die "unsupported non-file symlink: ${link}"
    rm -f "$link"
    cp -p "$target" "$link"
  done < <(find "$PYTHON_DIR" -type l | sort)
}

find_elfs() {
  find "$PYTHON_DIR" -type f -exec sh -c '
    for f do
      case "$(file -b "$f" 2>/dev/null)" in
        ELF*) printf "%s\n" "$f" ;;
      esac
    done
  ' sh {} +
}

strip_elfs() {
  local elf

  log "Stripping target ELF files"
  while IFS= read -r elf; do
    "$STRIP" --strip-unneeded "$elf" 2>/dev/null || true
  done < <(find_elfs)
}

copy_all_runtime_deps() {
  local elf

  log "Copying target runtime dependencies"
  copy_loader
  while IFS= read -r elf; do
    copy_runtime_deps "$elf"
  done < <(find_elfs)
}

write_python_wrapper() {
  local path="$1"
  local target="$2"
  local name

  name="$(basename "$path")"

  {
    printf '%s\n' '#!/bin/sh'
    printf '%s\n' 'PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"'
    printf '%s\n' 'PLUMOS_LIB="${PLUMOS_ROOT}/lib"'
    printf '%s\n' 'PLUMOS_PYTHON="${PLUMOS_ROOT}/python"'
    printf 'PLUMOS_ARGV0="${PLUMOS_ROOT}/bin/%s"\n' "$name"
    printf '%s\n' 'PLUMOS_LIBRARY_PATH="${PLUMOS_PYTHON}/lib:${PLUMOS_LIB}"'
    printf '%s\n' 'export PATH="${PLUMOS_PYTHON}/bin:${PLUMOS_ROOT}/bin:${PATH:-/bin:/usr/bin}"'
    printf '%s\n' 'exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \'
    printf '%s\n' '  --library-path "${PLUMOS_LIBRARY_PATH}:/usr/lib:/lib" \'
    printf '%s\n' '  --argv0 "${PLUMOS_ARGV0}" \'
    printf '  "${PLUMOS_PYTHON}/bin/%s" "$@"\n' "$target"
  } > "$path"
  chmod 0755 "$path"
}

write_module_wrapper() {
  local path="$1"
  local module="$2"

  {
    printf '%s\n' '#!/bin/sh'
    printf '%s\n' 'PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"'
    printf 'exec "${PLUMOS_ROOT}/bin/python3" -m %s "$@"\n' "$module"
  } > "$path"
  chmod 0755 "$path"
}

write_pip_wrapper() {
  local path="$1"

  {
    printf '%s\n' '#!/bin/sh'
    printf '%s\n' 'PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"'
    printf '%s\n' 'mkdir -p "${PLUMOS_ROOT}/cache/pip" 2>/dev/null || true'
    printf '%s\n' 'export PIP_CACHE_DIR="${PLUMOS_ROOT}/cache/pip"'
    printf '%s\n' 'export PIP_DISABLE_PIP_VERSION_CHECK=1'
    printf '%s\n' 'export PIP_ROOT_USER_ACTION=ignore'
    printf '%s\n' 'exec "${PLUMOS_ROOT}/bin/python3" -m pip "$@"'
  } > "$path"
  chmod 0755 "$path"
}

write_wrappers() {
  log "Writing loader wrappers"
  write_python_wrapper "${BIN_DIR}/python3" "python${PYTHON_MAJOR_MINOR}"
  write_python_wrapper "${BIN_DIR}/python${PYTHON_MAJOR_MINOR}" "python${PYTHON_MAJOR_MINOR}"
  write_pip_wrapper "${BIN_DIR}/pip3"
  write_pip_wrapper "${BIN_DIR}/pip${PYTHON_MAJOR_MINOR}"
  write_module_wrapper "${BIN_DIR}/pyxel" "pyxel"
}

write_sitecustomize() {
  local sitecustomize="${PYTHON_DIR}/lib/python${PYTHON_MAJOR_MINOR}/sitecustomize.py"

  log "Writing plumOS Python sitecustomize"
  cat > "$sitecustomize" <<'PY'
"""plumOS runtime adjustments for Python packages.

pip's vendored packaging checks sys.executable to decide whether armv7l is
hard-float compatible with manylinux wheels. plumOS intentionally exposes the
public /mnt/SDCARD/plumos/bin/python3 shell wrapper as sys.executable so Python
subprocesses keep using the plumOS loader path. Point the armhf ELF probe at
the real interpreter while leaving sys.executable wrapper-safe.
"""

import os


def _patch_pip_manylinux_armhf():
    try:
        from pip._vendor.packaging import _manylinux
    except Exception:
        return

    original = getattr(_manylinux, "_is_linux_armhf", None)
    if original is None or getattr(original, "_plumos_patched", False):
        return

    def is_linux_armhf(executable):
        if original(executable):
            return True
        real_python = os.environ.get(
            "PLUMOS_REAL_PYTHON",
            "/mnt/SDCARD/plumos/python/bin/python3.14",
        )
        return original(real_python)

    is_linux_armhf._plumos_patched = True
    _manylinux._is_linux_armhf = is_linux_armhf


_patch_pip_manylinux_armhf()
PY
  chmod 0644 "$sitecustomize"
}

write_manifest() {
  {
    printf 'python_version=%s\n' "$PYTHON_VERSION"
    printf 'python_url=%s\n' "$PYTHON_URL"
    printf 'python_sha256=%s\n' "$PYTHON_SHA256"
    printf 'target=%s\n' "$TARGET_GNU_TYPE"
    printf 'build=%s\n' "$BUILD_GNU_TYPE"
    printf 'prefix=%s\n' "$PLUMOS_PYTHON_PREFIX"
    printf 'cflags=%s\n' "$TARGET_CFLAGS"
    printf 'configure=--enable-shared --disable-test-modules --with-ensurepip=no\n'
    printf 'post_deploy=run /mnt/SDCARD/plumos/bin/python3 -m ensurepip --upgrade before pip use\n'
  } > "$MANIFEST"
}

main() {
  download_checked "$PYTHON_URL" "$TARBALL" "$PYTHON_SHA256"
  prepare_source
  build_host_python
  configure_target_python
  build_target_python
  install_target_python
  materialize_symlinks
  strip_elfs
  copy_all_runtime_deps
  write_wrappers
  write_sitecustomize
  write_manifest
  log "Python runtime staged at ${DIST_DIR}"
}

main "$@"
