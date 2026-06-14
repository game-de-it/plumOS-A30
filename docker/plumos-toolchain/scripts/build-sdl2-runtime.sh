#!/usr/bin/env bash
set -euo pipefail

SDL3_VERSION="${SDL3_VERSION:-3.4.10}"
SDL3_TAG="${SDL3_TAG:-release-${SDL3_VERSION}}"
SDL3_SHA256="${SDL3_SHA256:-12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785}"
SDL3_URL="${SDL3_URL:-https://github.com/libsdl-org/SDL/releases/download/${SDL3_TAG}/SDL3-${SDL3_VERSION}.tar.gz}"

SDL2_COMPAT_VERSION="${SDL2_COMPAT_VERSION:-2.32.68}"
SDL2_COMPAT_TAG="${SDL2_COMPAT_TAG:-release-${SDL2_COMPAT_VERSION}}"
SDL2_COMPAT_SHA256="${SDL2_COMPAT_SHA256:-401a64f5d0948f0d1a217cfdba4e72ce63d22f7a9fc3751251e0e3a175ff7703}"
SDL2_COMPAT_URL="${SDL2_COMPAT_URL:-https://github.com/libsdl-org/sdl2-compat/releases/download/${SDL2_COMPAT_TAG}/sdl2-compat-${SDL2_COMPAT_VERSION}.tar.gz}"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PATCH_DIR="${ROOT_DIR}/docker/plumos-toolchain/patches"
DOWNLOAD_DIR="${ROOT_DIR}/build/downloads"
BUILD_DIR="${ROOT_DIR}/build/sdl-upstream"
SDL3_SRC_DIR="${BUILD_DIR}/SDL3-${SDL3_VERSION}"
SDL3_BUILD_DIR="${BUILD_DIR}/SDL3-build"
SDL3_INSTALL_DIR="${BUILD_DIR}/SDL3-install"
SDL3_CONFIGURE_LOG="${BUILD_DIR}/SDL3-configure.log"
SDL2_COMPAT_SRC_DIR="${BUILD_DIR}/sdl2-compat-${SDL2_COMPAT_VERSION}"
SDL2_COMPAT_BUILD_DIR="${BUILD_DIR}/sdl2-compat-build"
SDL2_COMPAT_INSTALL_DIR="${BUILD_DIR}/sdl2-compat-install"
SDL2_COMPAT_CONFIGURE_LOG="${BUILD_DIR}/sdl2-compat-configure.log"
DIST_DIR="${ROOT_DIR}/dist/plumos-sdl2-runtime"
LIB_DIR="${DIST_DIR}/plumos/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/plumos-sdl2-runtime"
SDL3_TARBALL="${DOWNLOAD_DIR}/SDL3-${SDL3_VERSION}.tar.gz"
SDL2_COMPAT_TARBALL="${DOWNLOAD_DIR}/sdl2-compat-${SDL2_COMPAT_VERSION}.tar.gz"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

CC="${CC:-arm-linux-gnueabihf-gcc}"
CXX="${CXX:-arm-linux-gnueabihf-g++}"
STRIP="${STRIP:-arm-linux-gnueabihf-strip}"
READELF="${READELF:-arm-linux-gnueabihf-readelf}"

CFLAGS_TARGET="-march=${PLUMOS_MARCH:-armv7-a} -mfpu=${PLUMOS_MFPU:-neon-vfpv4} -mfloat-abi=${PLUMOS_MFLOAT_ABI:-hard}"

SDL3_CMAKE_OPTIONS=(
  -DCMAKE_SYSTEM_NAME=Linux
  -DCMAKE_SYSTEM_PROCESSOR=arm
  -DCMAKE_C_COMPILER="$CC"
  -DCMAKE_CXX_COMPILER="$CXX"
  -DCMAKE_INSTALL_PREFIX="$SDL3_INSTALL_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_FLAGS="$CFLAGS_TARGET"
  -DSDL_SHARED=ON
  -DSDL_STATIC=OFF
  -DSDL_TESTS=OFF
  -DSDL_EXAMPLES=OFF
  -DSDL_TEST_LIBRARY=OFF
  -DSDL_INSTALL_TESTS=OFF
  -DSDL_INSTALL=ON
  -DSDL_UNIX_CONSOLE_BUILD=ON
  -DSDL_OPENGL=OFF
  -DSDL_OPENGLES=ON
  -DSDL_VULKAN=OFF
  -DSDL_X11=OFF
  -DSDL_WAYLAND=OFF
  -DSDL_KMSDRM=OFF
  -DSDL_PIPEWIRE=OFF
  -DSDL_PULSEAUDIO=OFF
  -DSDL_ALSA=ON
  -DSDL_ALSA_SHARED=ON
  -DSDL_JACK=OFF
  -DSDL_LIBUDEV=OFF
  -DSDL_DBUS=OFF
  -DSDL_IBUS=OFF
  -DSDL_HIDAPI=OFF
)

SDL2_COMPAT_CMAKE_OPTIONS=(
  -DCMAKE_SYSTEM_NAME=Linux
  -DCMAKE_SYSTEM_PROCESSOR=arm
  -DCMAKE_C_COMPILER="$CC"
  -DCMAKE_CXX_COMPILER="$CXX"
  -DCMAKE_PREFIX_PATH="$SDL3_INSTALL_DIR"
  -DSDL3_DIR="${SDL3_INSTALL_DIR}/lib/cmake/SDL3"
  -DCMAKE_INSTALL_PREFIX="$SDL2_COMPAT_INSTALL_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_FLAGS="$CFLAGS_TARGET"
  -DBUILD_SHARED_LIBS=ON
  -DSDL2COMPAT_TESTS=OFF
)

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
  local tarball="$1"
  local src_dir="$2"

  rm -rf "$src_dir"
  mkdir -p "$src_dir"
  tar -C "$src_dir" --strip-components=1 -xf "$tarball"
}

apply_sdl3_patches() {
  log "Applying SDL3 A30 Mali video backend patch"
  patch -d "$SDL3_SRC_DIR" -p1 < "${PATCH_DIR}/sdl3-3.4.10-a30mali-video-driver.patch"
  patch -d "$SDL3_SRC_DIR" -p1 < "${PATCH_DIR}/sdl3-3.4.10-a30mali-rotation-present.patch"
}

build_sdl3() {
  log "Building SDL3 ${SDL3_VERSION}"
  rm -rf "$SDL3_BUILD_DIR" "$SDL3_INSTALL_DIR"

  cmake -S "$SDL3_SRC_DIR" -B "$SDL3_BUILD_DIR" -G Ninja \
    "${SDL3_CMAKE_OPTIONS[@]}" \
    2>&1 | tee "$SDL3_CONFIGURE_LOG"

  cmake --build "$SDL3_BUILD_DIR" -j"$JOBS"
  cmake --install "$SDL3_BUILD_DIR"
}

build_sdl2_compat() {
  log "Building sdl2-compat ${SDL2_COMPAT_VERSION}"
  rm -rf "$SDL2_COMPAT_BUILD_DIR" "$SDL2_COMPAT_INSTALL_DIR"

  cmake -S "$SDL2_COMPAT_SRC_DIR" -B "$SDL2_COMPAT_BUILD_DIR" -G Ninja \
    "${SDL2_COMPAT_CMAKE_OPTIONS[@]}" \
    2>&1 | tee "$SDL2_COMPAT_CONFIGURE_LOG"

  cmake --build "$SDL2_COMPAT_BUILD_DIR" -j"$JOBS"
  cmake --install "$SDL2_COMPAT_BUILD_DIR"
}

find_lib() {
  local soname="$1"
  local dir

  for dir in \
    "$SDL2_COMPAT_INSTALL_DIR/lib" \
    "$SDL3_INSTALL_DIR/lib" \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/arm-linux-gnueabihf/lib; do
    if [ -e "$dir/$soname" ]; then
      readlink -f "$dir/$soname"
      return 0
    fi
  done
  return 1
}

copy_regular_soname() {
  local path="$1"
  local soname="$2"
  local src

  src="$(readlink -f "$path")"
  install -m 0644 "$src" "$LIB_DIR/$(basename "$src")"
  if [ "$(basename "$src")" != "$soname" ]; then
    cp -f "$LIB_DIR/$(basename "$src")" "$LIB_DIR/$soname"
    chmod 0644 "$LIB_DIR/$soname"
  fi
}

copy_deps() {
  local elf="$1"
  local dep
  local src

  "$READELF" -d "$elf" 2>/dev/null | awk -F"[][]" "/NEEDED/ {print \$2}" | while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    if [ -e "$LIB_DIR/$dep" ]; then
      continue
    fi
    src="$(find_lib "$dep")" || {
      echo "warning: dependency not found: $dep" >&2
      continue
    }
    copy_regular_soname "$src" "$dep"
    copy_deps "$src"
  done
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

assemble_package() {
  local sdl2_lib="${SDL2_COMPAT_INSTALL_DIR}/lib/libSDL2-2.0.so.0"
  local sdl3_lib="${SDL3_INSTALL_DIR}/lib/libSDL3.so.0"

  log "Assembling SDL2 runtime package"
  rm -rf "$DIST_DIR"
  mkdir -p "$LIB_DIR" "$DOC_DIR"

  copy_loader
  copy_regular_soname "$sdl2_lib" "libSDL2-2.0.so.0"
  copy_regular_soname "$sdl2_lib" "libSDL2-2.0.so"
  copy_regular_soname "$sdl3_lib" "libSDL3.so.0"
  copy_regular_soname "$sdl3_lib" "libSDL3.so"
  copy_deps "$(readlink -f "$sdl2_lib")"
  copy_deps "$(readlink -f "$sdl3_lib")"

  "$STRIP" "$LIB_DIR"/libSDL*.so* 2>/dev/null || true

  sha256sum "$LIB_DIR"/* > "${DOC_DIR}/plumos-sdl2-runtime.sha256"

  {
    echo "plumOS SDL2 upstream runtime"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "cc: $("$CC" --version | head -n 1)"
    echo "sdl3_version: ${SDL3_VERSION}"
    echo "sdl3_tag: ${SDL3_TAG}"
    echo "sdl3_url: ${SDL3_URL}"
    echo "sdl3_sha256: ${SDL3_SHA256}"
    echo "sdl2_compat_version: ${SDL2_COMPAT_VERSION}"
    echo "sdl2_compat_tag: ${SDL2_COMPAT_TAG}"
    echo "sdl2_compat_url: ${SDL2_COMPAT_URL}"
    echo "sdl2_compat_sha256: ${SDL2_COMPAT_SHA256}"
    echo
    echo "== SDL3 cmake options =="
    printf '%s\n' "${SDL3_CMAKE_OPTIONS[@]}"
    echo
    echo "== sdl2-compat cmake options =="
    printf '%s\n' "${SDL2_COMPAT_CMAKE_OPTIONS[@]}"
    echo
    echo "== SDL3 enabled backends =="
    grep -E "Enabled backends:|Video drivers:|Render drivers:|Audio drivers:|Joystick drivers:" \
      "$SDL3_CONFIGURE_LOG" 2>/dev/null || true
    echo "plumOS custom video drivers: a30mali"
    echo
    echo "== needed =="
    "$READELF" -d "$(readlink -f "$sdl2_lib")" | grep "NEEDED" || true
    "$READELF" -d "$(readlink -f "$sdl3_lib")" | grep "NEEDED" || true
    echo
    echo "== bundled libs =="
    find "$LIB_DIR" -maxdepth 1 \( -type f -o -type l \) | sort | xargs -r -n1 basename
  } > "${DOC_DIR}/manifest.txt"

  printf 'Built %s\n' "$DIST_DIR"
  printf 'SDL3 %s + sdl2-compat %s\n' "$SDL3_VERSION" "$SDL2_COMPAT_VERSION"
}

download_checked "$SDL3_URL" "$SDL3_TARBALL" "$SDL3_SHA256"
download_checked "$SDL2_COMPAT_URL" "$SDL2_COMPAT_TARBALL" "$SDL2_COMPAT_SHA256"
prepare_source "$SDL3_TARBALL" "$SDL3_SRC_DIR"
prepare_source "$SDL2_COMPAT_TARBALL" "$SDL2_COMPAT_SRC_DIR"
apply_sdl3_patches
build_sdl3
build_sdl2_compat
assemble_package
