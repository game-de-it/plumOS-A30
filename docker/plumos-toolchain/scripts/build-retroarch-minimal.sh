#!/bin/sh
set -eu

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DOWNLOAD_DIR=${DOWNLOAD_DIR:-"${BUILD_ROOT}/downloads"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-retroarch-minimal"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
PATCH_DIR=${PATCH_DIR:-"${ROOT_DIR}/docker/plumos-toolchain/patches"}

RETROARCH_VERSION=${RETROARCH_VERSION:-1.22.2}
RETROARCH_TAG=${RETROARCH_TAG:-v${RETROARCH_VERSION}}
RETROARCH_SHA256=${RETROARCH_SHA256:-245ef18c8fa8fbd9fbb5eb25cf43e17c6aace2f95c1ed99873cbd794012bb232}
RETROARCH_URL=${RETROARCH_URL:-https://github.com/libretro/RetroArch/archive/refs/tags/${RETROARCH_TAG}.tar.gz}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
CXX=${CXX:-${CROSS_PREFIX}g++}
AR=${AR:-${CROSS_PREFIX}ar}
RANLIB=${RANLIB:-${CROSS_PREFIX}ranlib}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

ARCHIVE="${DOWNLOAD_DIR}/RetroArch-${RETROARCH_TAG}.tar.gz"
SRC_DIR="${BUILD_ROOT}/retroarch-${RETROARCH_VERSION}/src"
PREFIX="/mnt/SDCARD/plumos/retroarch"

msg() {
  printf '[retroarch-minimal] %s\n' "$*"
}

fetch_archive() {
  mkdir -p "${DOWNLOAD_DIR}"
  if [ ! -f "${ARCHIVE}" ]; then
    msg "fetching ${RETROARCH_URL}"
    curl -fsSL "${RETROARCH_URL}" -o "${ARCHIVE}"
  fi

  actual=$(sha256sum "${ARCHIVE}" | awk '{print $1}')
  if [ "${actual}" != "${RETROARCH_SHA256}" ]; then
    printf 'RetroArch archive checksum mismatch\nexpected: %s\nactual:   %s\n' \
      "${RETROARCH_SHA256}" "${actual}" >&2
    exit 1
  fi
}

prepare_source() {
  rm -rf "${SRC_DIR}"
  mkdir -p "$(dirname "${SRC_DIR}")"
  tar -xzf "${ARCHIVE}" -C "$(dirname "${SRC_DIR}")"
  mv "$(dirname "${SRC_DIR}")/RetroArch-${RETROARCH_VERSION}" "${SRC_DIR}"
}

apply_patches() {
  cd "${SRC_DIR}"
  msg "applying A30 display patches"
  patch -p1 < "${PATCH_DIR}/retroarch-1.22.2-a30-gl2-display-rotation.patch"
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

  loader=$(find_target_lib ld-linux-armhf.so.3 || true)
  if [ -n "${loader}" ]; then
    cp "${loader}" "${lib_dir}/"
  fi

  # Seed the common glibc pieces. readelf below expands this for optional deps.
  for lib in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libgcc_s.so.1; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    fi
  done

  needed=$("${READELF}" -d "${binary}" | awk '/NEEDED/ {gsub(/\[|\]/, "", $5); print $5}')
  for lib in ${needed}; do
    case "${lib}" in
      libEGL.so*|libGLESv2.so*|libMali.so*)
        # These are expected from the A30 stock rootfs at runtime.
        continue
        ;;
    esac
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    else
      msg "warning: could not locate runtime dependency ${lib}"
    fi
  done
}

build_retroarch() {
  cd "${SRC_DIR}"

  export CC CXX AR RANLIB STRIP
  export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
  export CFLAGS="${CFLAGS:-"-Os -pipe -marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"}"
  export CXXFLAGS="${CXXFLAGS:-"${CFLAGS}"}"
  export LDFLAGS="${LDFLAGS:-}"
  export HAVE_XKBCOMMON=no

  msg "configuring RetroArch ${RETROARCH_VERSION}"
  ./configure \
    --host=arm-linux-gnueabihf \
    --prefix="${PREFIX}" \
    --enable-neon \
    --enable-floathard \
    --enable-dylib \
    --enable-rgui \
    --enable-opengles \
    --enable-egl \
    --enable-mali_fbdev \
    --disable-opengl \
    --disable-opengl1 \
    --disable-opengl_core \
    --disable-vulkan \
    --disable-vulkan_display \
    --disable-kms \
    --disable-wayland \
    --disable-x11 \
    --disable-sdl \
    --disable-sdl2 \
    --disable-udev \
    --disable-alsa \
    --disable-oss \
    --disable-tinyalsa \
    --disable-audioio \
    --disable-rsound \
    --disable-roar \
    --disable-jack \
    --disable-pipewire \
    --disable-pulse \
    --disable-freetype \
    --disable-stb_font \
    --disable-nearest_resampler \
    --disable-cc_resampler \
    --disable-xmb \
    --disable-ozone \
    --disable-materialui \
    --disable-gfx_widgets \
    --disable-overlay \
    --disable-networking \
    --disable-networkgamepad \
    --disable-netplaydiscovery \
    --disable-cheevos \
    --disable-ssl \
    --disable-systemd \
    --disable-libusb \
    --disable-blissbox \
    --disable-builtinmbedtls \
    --disable-builtinbearssl \
    --disable-discord \
    --disable-ffmpeg \
    --disable-v4l2 \
    --disable-qt \
    --disable-xrandr \
    --disable-xscrnsaver \
    --disable-xi2 \
    --disable-xinerama \
    --disable-xshm \
    --disable-xvideo \
    --disable-libdecor \
    --disable-videocore \
    --disable-videoprocessor \
    --disable-vg \
    --disable-coreaudio \
    --disable-bsv_movie \
    --disable-runahead \
    --disable-rewind \
    --disable-screenshots \
    --disable-imageviewer \
    --disable-accessibility \
    --disable-cdrom \
    --disable-chd \
    --disable-flac \
    --disable-dr_mp3 \
    --disable-7zip \
    --disable-zstd \
    --disable-patch \
    --disable-xdelta \
    --disable-video_filter \
    --disable-dsp_filter \
    --disable-audiomixer \
    --disable-rpng \
    --disable-rbmp \
    --disable-rjpeg \
    --disable-rtga \
    --disable-libretrodb \
    --disable-core_info_cache \
    --disable-update_assets \
    --disable-update_cores \
    --disable-update_core_info \
    --disable-online_updater \
    --disable-cheats \
    --disable-translate \
    --disable-shaderpipeline \
    --disable-glx \
    --disable-slang \
    --disable-glslang \
    --disable-builtinglslang \
    --disable-spirv_cross \
    --disable-stb_image \
    --disable-stb_vorbis \
    --disable-ibxm \
    --disable-rwav \
    --disable-langextra \
    --disable-microphone \
    --disable-test_drivers \
    --disable-parport \
    --disable-rpiled \
    --disable-crtswitchres \
    --enable-builtinzlib

  msg "building with ${JOBS} job(s)"
  make -j"${JOBS}"
}

stage_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p \
    "${TARGET_DIR}/plumos/bin" \
    "${TARGET_DIR}/plumos/lib" \
    "${TARGET_DIR}/plumos/retroarch/bin" \
    "${TARGET_DIR}/plumos/retroarch/config" \
    "${TARGET_DIR}/plumos/retroarch/cores" \
    "${TARGET_DIR}/plumos/retroarch/home" \
    "${TARGET_DIR}/plumos/retroarch/info" \
    "${TARGET_DIR}/plumos/retroarch/logs" \
    "${TARGET_DIR}/plumos/retroarch/playlists/builtin" \
    "${TARGET_DIR}/plumos/retroarch/saves" \
    "${TARGET_DIR}/plumos/retroarch/states" \
    "${TARGET_DIR}/plumos/retroarch/system" \
    "${TARGET_DIR}/docs"

  cp "${SRC_DIR}/retroarch" "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin"
  "${STRIP}" "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" || true
  copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" "${TARGET_DIR}/plumos/lib"

  cat > "${TARGET_DIR}/plumos/retroarch/config/retroarch-minimal.cfg" <<'EOF'
video_driver = "gl"
video_context_driver = "mali_fbdev"
video_fullscreen = "true"
video_vsync = "false"
video_threaded = "false"
video_smooth = "false"
video_shader_enable = "false"
video_rotation = "1"
screen_orientation = "1"

audio_enable = "false"
audio_driver = "null"
input_driver = "null"
input_joypad_driver = "null"
input_autodetect_enable = "false"
input_max_users = "1"

menu_driver = "rgui"
menu_enable_widgets = "false"
menu_show_online_updater = "false"
menu_show_load_core = "false"
menu_show_load_content = "false"
menu_show_information = "true"
history_list_enable = "false"
config_save_on_exit = "false"
gamemode_enable = "false"

assets_directory = "/mnt/SDCARD/plumos/retroarch/assets"
libretro_directory = "/mnt/SDCARD/plumos/retroarch/cores"
libretro_info_path = "/mnt/SDCARD/plumos/retroarch/info"
log_dir = "/mnt/SDCARD/plumos/retroarch/logs"
playlist_directory = "/mnt/SDCARD/plumos/retroarch/playlists"
content_favorites_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_favorites.lpl"
content_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_history.lpl"
content_image_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_image_history.lpl"
content_music_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_music_history.lpl"
content_video_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_video_history.lpl"
savefile_directory = "/mnt/SDCARD/plumos/retroarch/saves"
savestate_directory = "/mnt/SDCARD/plumos/retroarch/states"
system_directory = "/mnt/SDCARD/plumos/retroarch/system"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/bin/retroarch" <<'EOF'
#!/bin/sh
set -eu

PLUMOS_ROOT=${PLUMOS_ROOT:-/mnt/SDCARD/plumos}
PLUMOS_LIB="${PLUMOS_ROOT}/lib"
RETROARCH_BIN="${PLUMOS_ROOT}/retroarch/bin/retroarch.bin"
export HOME="${PLUMOS_ROOT}/retroarch/home"
export PLUMOS_RA_DISPLAY_ROTATION="${PLUMOS_RA_DISPLAY_ROTATION:-ccw}"
mkdir -p "${HOME}"

exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \
  --library-path "${PLUMOS_LIB}:/usr/lib:/lib" \
  "${RETROARCH_BIN}" "$@"
EOF
  chmod +x "${TARGET_DIR}/plumos/retroarch/bin/retroarch"

  cat > "${TARGET_DIR}/plumos/bin/plumos-retroarch-minimal" <<'EOF'
#!/bin/sh
set -eu

exec /mnt/SDCARD/plumos/retroarch/bin/retroarch \
  --menu \
  --verbose \
  --config /mnt/SDCARD/plumos/retroarch/config/retroarch-minimal.cfg \
  "$@"
EOF
  chmod +x "${TARGET_DIR}/plumos/bin/plumos-retroarch-minimal"

  cp "${SRC_DIR}/COPYING" "${TARGET_DIR}/docs/RetroArch-COPYING"

  {
    printf 'RetroArch minimal A30 display probe build\n'
    printf 'version=%s\n' "${RETROARCH_VERSION}"
    printf 'tag=%s\n' "${RETROARCH_TAG}"
    printf 'source=%s\n' "${RETROARCH_URL}"
    printf 'sha256=%s\n' "${RETROARCH_SHA256}"
    printf 'configure=GLESv2/EGL Mali fbdev RGUI only; stock SDL/X11/Wayland/audio/networking/heavy menus disabled\n'
    printf '\nNEEDED:\n'
    "${READELF}" -d "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" | awk '/NEEDED/ {print}'
  } > "${TARGET_DIR}/docs/manifest.txt"

  msg "staged ${TARGET_DIR}"
}

fetch_archive
prepare_source
apply_patches
build_retroarch
stage_dist
