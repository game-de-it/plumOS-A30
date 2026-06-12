#!/bin/sh
set -eu

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DOWNLOAD_DIR=${DOWNLOAD_DIR:-"${BUILD_ROOT}/downloads"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-retroarch-practical"}
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
SRC_DIR="${BUILD_ROOT}/retroarch-${RETROARCH_VERSION}-practical/src"
PREFIX="/mnt/SDCARD/plumos/retroarch"
SDL2_PREFIX=${SDL2_PREFIX:-"${ROOT_DIR}/build/sdl-upstream/sdl2-compat-install"}
SDL3_PREFIX=${SDL3_PREFIX:-"${ROOT_DIR}/build/sdl-upstream/SDL3-install"}

msg() {
  printf '[retroarch-practical] %s\n' "$*"
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
  patch -p1 < "${PATCH_DIR}/retroarch-1.22.2-a30-landscape-fbo.patch"
  patch -p1 < "${PATCH_DIR}/retroarch-1.22.2-a30-restart-exec-path.patch"
  patch -p1 < "${PATCH_DIR}/retroarch-1.22.2-nci-save-state-slot.patch"
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
    "${SDL2_PREFIX}/lib" \
    "${SDL3_PREFIX}/lib" \
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

ensure_sdl2_runtime() {
  if [ ! -x "${SDL2_PREFIX}/bin/sdl2-config" ] ||
     [ ! -f "${SDL2_PREFIX}/lib/libSDL2-2.0.so.0" ] ||
     [ ! -f "${SDL3_PREFIX}/lib/libSDL3.so.0" ]; then
    msg "building SDL3+sdl2-compat runtime for SDL2 joypad input"
    "${ROOT_DIR}/docker/plumos-toolchain/scripts/build-sdl2-runtime.sh"
  fi

  if [ -f "${SDL2_PREFIX}/lib/pkgconfig/sdl2-compat.pc" ] &&
     [ ! -e "${SDL2_PREFIX}/lib/pkgconfig/sdl2.pc" ]; then
    ln -sf sdl2-compat.pc "${SDL2_PREFIX}/lib/pkgconfig/sdl2.pc"
  fi
}

copy_runtime_deps() {
  binary=$1
  lib_dir=$2
  mkdir -p "${lib_dir}"

  loader=$(find_target_lib ld-linux-armhf.so.3 || true)
  if [ -n "${loader}" ]; then
    cp "${loader}" "${lib_dir}/"
  fi

  for lib in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libgcc_s.so.1 libstdc++.so.6; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}"
    fi
  done

  needed=$("${READELF}" -d "${binary}" | awk '/NEEDED/ {gsub(/\[|\]/, "", $5); print $5}')
  for lib in ${needed}; do
    case "${lib}" in
      libEGL.so*|libGLESv2.so*|libMali.so*)
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
  export PATH="${SDL2_PREFIX}/bin:${PATH}"
  export SDL2_CONFIG="${SDL2_PREFIX}/bin/sdl2-config"
  export PKG_CONFIG_LIBDIR="${SDL2_PREFIX}/lib/pkgconfig:${SDL3_PREFIX}/lib/pkgconfig:/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig"
  export CFLAGS="${CFLAGS:-"-O2 -pipe -marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fomit-frame-pointer"}"
  export CXXFLAGS="${CXXFLAGS:-"${CFLAGS}"}"
  export LDFLAGS="${LDFLAGS:-"-L${SDL2_PREFIX}/lib -L${SDL3_PREFIX}/lib"}"
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
    --enable-threads \
    --enable-zlib \
    --enable-builtinzlib \
    --enable-networking \
    --enable-command \
    --enable-alsa \
    --enable-oss \
    --enable-udev \
    --enable-sdl2 \
    --disable-opengl \
    --disable-opengl1 \
    --disable-opengl_core \
    --disable-vulkan \
    --disable-vulkan_display \
    --disable-kms \
    --disable-wayland \
    --disable-x11 \
    --disable-sdl \
    --disable-tinyalsa \
    --disable-audioio \
    --disable-rsound \
    --disable-roar \
    --disable-jack \
    --disable-pipewire \
    --disable-pulse \
    --disable-freetype \
    --disable-xmb \
    --disable-ozone \
    --disable-materialui \
    --disable-gfx_widgets \
    --disable-overlay \
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
    --disable-accessibility \
    --disable-cdrom \
    --disable-chd \
    --disable-flac \
    --disable-zstd \
    --disable-xdelta \
    --disable-video_filter \
    --disable-dsp_filter \
    --disable-audiomixer \
    --disable-rbmp \
    --disable-rtga \
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
    --disable-stb_vorbis \
    --disable-ibxm \
    --disable-rwav \
    --disable-langextra \
    --disable-microphone \
    --disable-test_drivers \
    --disable-parport \
    --disable-rpiled \
    --disable-crtswitchres

  msg "building with ${JOBS} job(s)"
  make -j"${JOBS}"
}

stage_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p \
    "${TARGET_DIR}/plumos/bin" \
    "${TARGET_DIR}/plumos/lib" \
    "${TARGET_DIR}/plumos/retroarch/autoconfig/linuxraw" \
    "${TARGET_DIR}/plumos/retroarch/autoconfig/sdl2" \
    "${TARGET_DIR}/plumos/retroarch/autoconfig/udev" \
    "${TARGET_DIR}/plumos/retroarch/bin" \
    "${TARGET_DIR}/plumos/retroarch/config" \
    "${TARGET_DIR}/plumos/retroarch/cores" \
    "${TARGET_DIR}/plumos/retroarch/home" \
    "${TARGET_DIR}/plumos/retroarch/info" \
    "${TARGET_DIR}/plumos/retroarch/logs" \
    "${TARGET_DIR}/plumos/retroarch/playlists/builtin" \
    "${TARGET_DIR}/plumos/retroarch/saves" \
    "${TARGET_DIR}/plumos/retroarch/screenshots" \
    "${TARGET_DIR}/plumos/retroarch/states" \
    "${TARGET_DIR}/plumos/retroarch/system" \
    "${TARGET_DIR}/docs"

  cp "${SRC_DIR}/retroarch" "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin"
  "${STRIP}" "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" || true
  copy_runtime_deps "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" "${TARGET_DIR}/plumos/lib"
  for lib in libSDL2-2.0.so.0 libSDL3.so.0; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${TARGET_DIR}/plumos/lib"
    fi
  done

  cat > "${TARGET_DIR}/plumos/retroarch/config/retroarch-practical.cfg" <<'EOF'
video_driver = "gl"
video_context_driver = "mali_fbdev"
video_fullscreen = "true"
video_vsync = "true"
vrr_runloop_enable = "true"
video_threaded = "false"
video_smooth = "false"
video_shader_enable = "false"
video_rotation = "0"
screen_orientation = "0"

audio_enable = "true"
audio_driver = "alsa"
audio_device = "default"
audio_latency = "128"
audio_sync = "true"
audio_rate_control = "true"
audio_mute_enable = "false"

input_driver = "null"
input_joypad_driver = "sdl2"
input_autodetect_enable = "true"
input_max_users = "1"
input_player1_joypad_index = "0"
input_player1_a_btn = "1"
input_player1_b_btn = "0"
input_player1_x_btn = "3"
input_player1_y_btn = "2"
input_player1_l_btn = "9"
input_player1_r_btn = "10"
input_player1_select_btn = "4"
input_player1_start_btn = "6"
input_player1_up_btn = "11"
input_player1_down_btn = "12"
input_player1_left_btn = "13"
input_player1_right_btn = "14"
input_player1_l_x_plus_axis = "+0"
input_player1_l_x_minus_axis = "-0"
input_player1_l_y_plus_axis = "+1"
input_player1_l_y_minus_axis = "-1"
input_axis_threshold = "0.500000"
input_enable_hotkey_btn = "4"
input_menu_toggle_btn = "0"
input_exit_emulator_btn = "6"
quit_press_twice = "false"

joypad_autoconfig_dir = "/mnt/SDCARD/plumos/retroarch/autoconfig"
menu_driver = "rgui"
menu_enable_widgets = "false"
menu_show_online_updater = "false"
menu_show_load_core = "false"
menu_show_load_content = "false"
menu_show_information = "true"
content_show_history = "false"
history_list_enable = "false"
config_save_on_exit = "false"
gamemode_enable = "false"

network_cmd_enable = "true"
network_cmd_port = "55355"

assets_directory = "/mnt/SDCARD/plumos/retroarch/assets"
libretro_directory = "/mnt/SDCARD/plumos/retroarch/cores"
libretro_info_path = "/mnt/SDCARD/plumos/retroarch/info"
log_dir = "/mnt/SDCARD/plumos/retroarch/logs"
playlist_directory = "/mnt/SDCARD/plumos/retroarch/playlists"
screenshot_directory = "/mnt/SDCARD/plumos/retroarch/screenshots"
content_favorites_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_favorites.lpl"
content_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_history.lpl"
content_image_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_image_history.lpl"
content_music_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_music_history.lpl"
content_video_history_path = "/mnt/SDCARD/plumos/retroarch/playlists/builtin/content_video_history.lpl"
savefile_directory = "/mnt/SDCARD/plumos/retroarch/saves"
savestate_directory = "/mnt/SDCARD/plumos/retroarch/states"
system_directory = "/mnt/SDCARD/plumos/retroarch/system"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/config/retroarch-practical-alsa-append.cfg" <<'EOF'
audio_driver = "alsa"
audio_device = "default"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/autoconfig/sdl2/plumOS A30 Gamepad.cfg" <<'EOF'
input_driver = "sdl2"
input_device = "plumOS A30 Gamepad"
input_device_display_name = "plumOS A30 Gamepad"
input_b_btn = "0"
input_a_btn = "1"
input_y_btn = "2"
input_x_btn = "3"
input_select_btn = "4"
input_start_btn = "6"
input_l_btn = "9"
input_r_btn = "10"
input_up_btn = "11"
input_down_btn = "12"
input_left_btn = "13"
input_right_btn = "14"
input_l2_axis = "+4"
input_r2_axis = "+5"
input_l3_btn = "7"
input_r3_btn = "8"
input_l_x_plus_axis = "+0"
input_l_x_minus_axis = "-0"
input_l_y_plus_axis = "+1"
input_l_y_minus_axis = "-1"
input_r_x_plus_axis = "+2"
input_r_x_minus_axis = "-2"
input_r_y_plus_axis = "-3"
input_r_y_minus_axis = "+3"
input_enable_hotkey_btn = "4"
input_menu_toggle_btn = "0"
input_exit_emulator_btn = "6"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/autoconfig/sdl2/Xbox 360 Controller.cfg" <<'EOF'
input_driver = "sdl2"
input_device = "Xbox 360 Controller"
input_device_display_name = "plumOS A30 Gamepad"
input_vendor_id = "1118"
input_product_id = "654"
input_b_btn = "0"
input_a_btn = "1"
input_y_btn = "2"
input_x_btn = "3"
input_select_btn = "4"
input_start_btn = "6"
input_l_btn = "9"
input_r_btn = "10"
input_up_btn = "11"
input_down_btn = "12"
input_left_btn = "13"
input_right_btn = "14"
input_l2_axis = "+4"
input_r2_axis = "+5"
input_l3_btn = "7"
input_r3_btn = "8"
input_l_x_plus_axis = "+0"
input_l_x_minus_axis = "-0"
input_l_y_plus_axis = "+1"
input_l_y_minus_axis = "-1"
input_r_x_plus_axis = "+2"
input_r_x_minus_axis = "-2"
input_r_y_plus_axis = "-3"
input_r_y_minus_axis = "+3"
input_enable_hotkey_btn = "4"
input_menu_toggle_btn = "0"
input_exit_emulator_btn = "6"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/autoconfig/linuxraw/plumOS A30 Gamepad.cfg" <<'EOF'
input_driver = "linuxraw"
input_device = "plumOS A30 Gamepad"
input_device_display_name = "plumOS A30 Gamepad"
input_vendor_id = "1118"
input_product_id = "654"
input_b_btn = "1"
input_y_btn = "3"
input_select_btn = "6"
input_start_btn = "7"
input_up_axis = "-7"
input_down_axis = "+7"
input_left_axis = "-6"
input_right_axis = "+6"
input_a_btn = "0"
input_x_btn = "2"
input_l_btn = "4"
input_r_btn = "5"
input_l2_axis = "+2"
input_r2_axis = "+5"
input_l3_btn = "9"
input_r3_btn = "10"
input_l_x_plus_axis = "+0"
input_l_x_minus_axis = "-0"
input_l_y_plus_axis = "+1"
input_l_y_minus_axis = "-1"
input_enable_hotkey_btn = "6"
input_menu_toggle_btn = "1"
input_exit_emulator_btn = "7"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/autoconfig/udev/plumOS A30 Gamepad.cfg" <<'EOF'
input_driver = "udev"
input_device = "plumOS A30 Gamepad"
input_device_display_name = "plumOS A30 Gamepad"
input_vendor_id = "1118"
input_product_id = "654"
input_b_btn = "1"
input_y_btn = "3"
input_select_btn = "6"
input_start_btn = "7"
input_up_axis = "-7"
input_down_axis = "+7"
input_left_axis = "-6"
input_right_axis = "+6"
input_a_btn = "0"
input_x_btn = "2"
input_l_btn = "4"
input_r_btn = "5"
input_l2_axis = "+2"
input_r2_axis = "+5"
input_l3_btn = "9"
input_r3_btn = "10"
input_l_x_plus_axis = "+0"
input_l_x_minus_axis = "-0"
input_l_y_plus_axis = "+1"
input_l_y_minus_axis = "-1"
input_enable_hotkey_btn = "6"
input_menu_toggle_btn = "1"
input_exit_emulator_btn = "7"
EOF

  cat > "${TARGET_DIR}/plumos/retroarch/bin/retroarch" <<'EOF'
#!/bin/sh
set -eu

PLUMOS_ROOT=${PLUMOS_ROOT:-/mnt/SDCARD/plumos}
PLUMOS_LIB="${PLUMOS_ROOT}/lib"
RETROARCH_BIN="${PLUMOS_ROOT}/retroarch/bin/retroarch.bin"
export HOME="${PLUMOS_ROOT}/retroarch/home"
export PLUMOS_RA_DISPLAY_ROTATION="${PLUMOS_RA_DISPLAY_ROTATION:-none}"
export PLUMOS_RA_LANDSCAPE_MODE="${PLUMOS_RA_LANDSCAPE_MODE:-fbo}"
export PLUMOS_RA_EXEC_PATH="${PLUMOS_RA_EXEC_PATH:-${RETROARCH_BIN}}"
export PLUMOS_RA_LD_PATH="${PLUMOS_RA_LD_PATH:-${PLUMOS_LIB}/ld-linux-armhf.so.3}"
export PLUMOS_RA_LIBRARY_PATH="${PLUMOS_RA_LIBRARY_PATH:-${PLUMOS_LIB}:/usr/lib:/lib}"
export PLUMOS_RA_CONFIG_PATH="${PLUMOS_RA_CONFIG_PATH:-${PLUMOS_ROOT}/retroarch/config/retroarch-practical.cfg}"
export PLUMOS_RA_RESTART_APPEND_PATH="${PLUMOS_RA_RESTART_APPEND_PATH:-/tmp/plumos-retroarch-restart.cfg}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
mkdir -p "${HOME}"

exec "${PLUMOS_RA_LD_PATH}" \
  --argv0 "${RETROARCH_BIN}" \
  --library-path "${PLUMOS_RA_LIBRARY_PATH}" \
  "${RETROARCH_BIN}" "$@"
EOF
  chmod +x "${TARGET_DIR}/plumos/retroarch/bin/retroarch"

  cat > "${TARGET_DIR}/plumos/bin/plumos-retroarch-practical" <<'EOF'
#!/bin/sh
set -eu

exec /mnt/SDCARD/plumos/retroarch/bin/retroarch \
  --verbose \
  --config /mnt/SDCARD/plumos/retroarch/config/retroarch-practical.cfg \
  "$@"
EOF
  chmod +x "${TARGET_DIR}/plumos/bin/plumos-retroarch-practical"

  cat > "${TARGET_DIR}/plumos/bin/plumos-retroarch-launch" <<'EOF'
#!/bin/sh
set -eu

PLUMOS_ROOT=${PLUMOS_ROOT:-/mnt/SDCARD/plumos}
RA="$PLUMOS_ROOT/bin/plumos-retroarch-practical"
JOY="$PLUMOS_ROOT/bin/plumos-joystickd"
UDP="$PLUMOS_ROOT/bin/plumos-udp-send"
VOLUME_CONTROL="$PLUMOS_ROOT/bin/plumos-volume-control"
LOG_DIR="$PLUMOS_ROOT/retroarch/logs"
APPEND="/tmp/plumos-retroarch-launch-$$.cfg"
CPU_STATE="/tmp/plumos-retroarch-launch-cpustate-$$"
SYSTEM_SETTINGS="$PLUMOS_ROOT/config/system/settings.json"
RA_LOG="$LOG_DIR/launch-last.log"
JOY_LOG="$LOG_DIR/launch-joystickd-last.log"
NETCMD_LOG="$LOG_DIR/launch-netcmd-last.log"
CORE=""
ROM=""
SYSTEM=""
AUDIO="${PLUMOS_RA_AUDIO:-alsa}"
AUDIO_LATENCY="${PLUMOS_RA_AUDIO_LATENCY:-}"
CPU_POLICY="${PLUMOS_RA_CPU_POLICY:-fixed}"
CPU_FREQ="${PLUMOS_RA_CPU_FREQ:-648000}"
CPU_CORES="${PLUMOS_RA_CPU_CORES:-2}"
ROTATION="${PLUMOS_RA_DISPLAY_ROTATION:-none}"
LANDSCAPE_MODE="${PLUMOS_RA_LANDSCAPE_MODE:-fbo}"
VRR_RUNLOOP="${PLUMOS_RA_VRR_RUNLOOP:-true}"
MAX_FRAMES="${PLUMOS_RA_MAX_FRAMES:-}"
QUIT_PRESS_TWICE="${PLUMOS_RA_QUIT_PRESS_TWICE:-}"
ENTRY_SLOT="${PLUMOS_RA_ENTRY_SLOT:-}"
SAFE_STATE_SLOT="${PLUMOS_RA_SAFE_STATE_SLOT:-999}"
SAFE_EXIT="${PLUMOS_RA_SAFE_EXIT:-1}"
SAFE_EXIT_SAVE_STATE="${PLUMOS_RA_SAFE_EXIT_SAVE_STATE:-1}"
SAFE_EXIT_SAVE_FILES="${PLUMOS_RA_SAFE_EXIT_SAVE_FILES:-1}"
SAFE_EXIT_CLOSE_CONTENT="${PLUMOS_RA_SAFE_EXIT_CLOSE_CONTENT:-1}"
DOSBOX_PURE_FORCE60FPS="${PLUMOS_RA_DOSBOX_PURE_FORCE60FPS:-}"
DOSBOX_PURE_CYCLES="${PLUMOS_RA_DOSBOX_PURE_CYCLES:-}"
USE_JOYSTICKD=1
JOY_PID=""
GAMEPAD_JS=""
RA_PID=""

usage() {
  cat <<USAGE
Usage: plumos-retroarch-launch --core CORE --rom ROM [options]
       plumos-retroarch-launch --system nes|gb [--rom ROM] [options]

Options:
  --audio alsa|oss          Audio driver. Default: ${AUDIO}
  --audio-latency MS        RetroArch audio latency override.
  --cpu performance|fixed
                            CPU policy. Default: ${CPU_POLICY}
  --freq KHZ                Fixed CPU frequency in kHz. Default: ${CPU_FREQ}
  --cores 2|4               CPU online core policy. Default: ${CPU_CORES}
  --rotation VALUE          PLUMOS_RA_DISPLAY_ROTATION. Default: ${ROTATION}
  --landscape-mode VALUE    A30 landscape present mode. Default: ${LANDSCAPE_MODE}
  --vrr-runloop true|false  Set RetroArch vrr_runloop_enable. Default: ${VRR_RUNLOOP}
  --max-frames N            Exit RetroArch after N frames. Default: disabled.
  --quit-press-twice true|false
                            Override RetroArch quit_press_twice for this launch.
  --entry-slot N            Load RetroArch entry savestate slot N on startup.
                            Default: disabled.
  --safe-state-slot N       RetroArch savestate slot reserved for safe
                            shutdown/resume. Default: ${SAFE_STATE_SLOT}
  --safe-exit true|false    On launcher TERM, use NCI save/close/quit before
                            fallback kill. Default: ${SAFE_EXIT}
  --dosbox-pure-force60fps true|false
                            Set dosbox_pure_force60fps for this launch.
  --dosbox-pure-cycles auto|max|N
                            Set dosbox_pure_cycles for this launch.
  --no-joystickd            Do not start plumos-joystickd.
  -h, --help                Show this help.
USAGE
}

first_rom() {
  dir="$1"
  pattern="$2"
  find "$dir" -maxdepth 2 -type f -name "$pattern" 2>/dev/null |
    grep -v "/\\._" |
    head -n 1
}

input_device_name() {
  dev="$1"
  base="${dev##*/}"
  cat "/sys/class/input/$base/device/name" 2>/dev/null || true
}

find_gamepad_js() {
  for dev in /dev/input/js*; do
    [ -e "$dev" ] || continue
    if [ "$(input_device_name "$dev")" = "plumOS A30 Gamepad" ]; then
      echo "$dev"
      return 0
    fi
  done
  return 1
}

wait_for_gamepad_js() {
  i=0
  while [ "$i" -lt 30 ]; do
    GAMEPAD_JS="$(find_gamepad_js || true)"
    [ -n "$GAMEPAD_JS" ] && return 0
    i=$((i + 1))
    sleep 0.1
  done
  return 1
}

save_cpu_policy() {
  p=/sys/devices/system/cpu/cpu0/cpufreq
  {
    printf "governor=%s\n" "$(cat "$p/scaling_governor" 2>/dev/null || true)"
    printf "min_freq=%s\n" "$(cat "$p/scaling_min_freq" 2>/dev/null || true)"
    printf "max_freq=%s\n" "$(cat "$p/scaling_max_freq" 2>/dev/null || true)"
    for cpu in 1 2 3; do
      if [ -r "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
        printf "cpu%s_online=%s\n" "$cpu" "$(cat "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true)"
      fi
    done
  } > "$CPU_STATE"
}

restore_cpu_policy() {
  [ -f "$CPU_STATE" ] || return 0
  # shellcheck disable=SC1090
  . "$CPU_STATE" 2>/dev/null || true

  for cpu in 1 2 3; do
    eval value="\${cpu${cpu}_online:-}"
    if [ -n "$value" ] && [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
      echo "$value" > "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
    fi
  done

  p=/sys/devices/system/cpu/cpu0/cpufreq
  if [ -n "${max_freq:-}" ] && [ -w "$p/scaling_max_freq" ]; then
    echo "$max_freq" > "$p/scaling_max_freq" 2>/dev/null || true
  fi
  if [ -n "${min_freq:-}" ] && [ -w "$p/scaling_min_freq" ]; then
    echo "$min_freq" > "$p/scaling_min_freq" 2>/dev/null || true
  fi
  if [ -n "${governor:-}" ] && [ -w "$p/scaling_governor" ]; then
    echo "$governor" > "$p/scaling_governor" 2>/dev/null || true
  fi
  rm -f "$CPU_STATE"
}

apply_cpu_policy() {
  save_cpu_policy
  case "$CPU_CORES" in
    2)
      [ -w /sys/devices/system/cpu/cpu1/online ] &&
        echo 1 > /sys/devices/system/cpu/cpu1/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu2/online ] &&
        echo 0 > /sys/devices/system/cpu/cpu2/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu3/online ] &&
        echo 0 > /sys/devices/system/cpu/cpu3/online 2>/dev/null || true
      ;;
    4)
      for cpu in 1 2 3; do
        [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ] &&
          echo 1 > "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
      done
      ;;
    *)
      echo "error: invalid CPU core policy: $CPU_CORES" >&2
      exit 2
      ;;
  esac
  case "$CPU_POLICY" in
    performance)
      echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
      ;;
    fixed)
      p=/sys/devices/system/cpu/cpu0/cpufreq
      echo userspace > "$p/scaling_governor" 2>/dev/null || true
      echo "$CPU_FREQ" > "$p/scaling_setspeed" 2>/dev/null || true
      echo "$CPU_FREQ" > "$p/scaling_min_freq" 2>/dev/null || true
      echo "$CPU_FREQ" > "$p/scaling_max_freq" 2>/dev/null || true
      ;;
    *)
      echo "error: invalid CPU policy: $CPU_POLICY" >&2
      exit 2
      ;;
  esac
}

ensure_loopback_up() {
  if [ -x "$PLUMOS_ROOT/gnu/bin/ip" ]; then
    "$PLUMOS_ROOT/gnu/bin/ip" link set lo up 2>/dev/null || true
  elif [ -x "$PLUMOS_ROOT/bin/ip" ]; then
    "$PLUMOS_ROOT/bin/ip" link set lo up 2>/dev/null || true
  fi
}

is_enabled() {
  case "$1" in
    1|yes|true|on) return 0 ;;
    *) return 1 ;;
  esac
}

validate_state_slot() {
  name=$1
  value=$2
  case "$value" in
    0|[1-9]|[1-9][0-9]|[1-9][0-9][0-9])
      ;;
    *)
      echo "error: invalid $name: $value" >&2
      exit 2
      ;;
  esac
}

read_system_volume() {
  value=""
  if [ -r "$SYSTEM_SETTINGS" ]; then
    value="$(sed -n 's/.*"volume"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$SYSTEM_SETTINGS" 2>/dev/null | head -n 1)"
  fi
  case "$value" in
    ""|*[!0-9]*) value=20 ;;
  esac
  value="$(printf '%s' "$value" | sed 's/^0*//')"
  [ -n "$value" ] || value=0
  if [ "$value" -lt 0 ]; then
    value=0
  elif [ "$value" -gt 20 ]; then
    value=20
  fi
  printf '%s\n' "$value"
}

retroarch_audio_volume_db() {
  volume=$1
  if [ "$volume" -le 0 ]; then
    printf '%s\n' "-80.000000"
  elif [ "$volume" -ge 20 ]; then
    printf '%s\n' "0.000000"
  else
    printf '%d.000000\n' $(( (volume - 20) * 2 ))
  fi
}

apply_system_volume() {
  if [ -x "$VOLUME_CONTROL" ]; then
    "$VOLUME_CONTROL" apply >>"$LOG_DIR/volume-control-last.log" 2>&1 || true
  fi
}

wait_for_retroarch_exit() {
  i=0
  while [ "$i" -lt 40 ]; do
    if [ -z "$RA_PID" ] || ! kill -0 "$RA_PID" 2>/dev/null; then
      return 0
    fi
    i=$((i + 1))
    sleep 0.25
  done
  return 1
}

netcmd_send() {
  [ -x "$UDP" ] || return 1
  "$UDP" "$@" >>"$NETCMD_LOG" 2>&1
}

safe_exit_retroarch() {
  [ -n "$RA_PID" ] && kill -0 "$RA_PID" 2>/dev/null || return 1
  is_enabled "$SAFE_EXIT" || return 1

  {
    echo "== safe exit $(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || true) =="
    echo "ra_pid=$RA_PID"
    echo "safe_state_slot=$SAFE_STATE_SLOT"
  } >>"$NETCMD_LOG" 2>&1

  ensure_loopback_up
  if is_enabled "$SAFE_EXIT_SAVE_STATE"; then
    netcmd_send "SAVE_STATE_SLOT $SAFE_STATE_SLOT" || true
    sleep 1
  fi
  if is_enabled "$SAFE_EXIT_SAVE_FILES"; then
    netcmd_send SAVE_FILES || true
    sleep 1
  fi
  if is_enabled "$SAFE_EXIT_CLOSE_CONTENT"; then
    netcmd_send CLOSE_CONTENT || true
    sleep 1
    netcmd_send CLOSE_CONTENT || true
    sleep 1
    netcmd_send --recv-ms 1000 GET_STATUS || true
  fi
  netcmd_send --repeat 2 --delay-ms 500 QUIT || true
  wait_for_retroarch_exit
}

signal_pid_quick() {
  pid=$1
  case "$pid" in
    ""|*[!0-9]*) return 0 ;;
  esac
  [ "$pid" = "$$" ] && return 0
  kill -TERM "$pid" 2>/dev/null || true
  kill -KILL "$pid" 2>/dev/null || true
}

kill_pid_quick() {
  pid=$1
  signal_pid_quick "$pid"
  wait "$pid" 2>/dev/null || true
}

stop_joystickd() {
  if [ -n "$JOY_PID" ]; then
    kill_pid_quick "$JOY_PID"
  fi
  for f in /proc/[0-9]*/cmdline; do
    [ -r "$f" ] || continue
    pid=${f#/proc/}
    pid=${pid%/cmdline}
    [ "$pid" = "$$" ] && continue
    cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
    case "$cmd" in
      *"$JOY --device-mode xbox"*) kill_pid_quick "$pid" ;;
    esac
  done
  JOY_PID=""
}

cleanup() {
  rc=$?
  trap - EXIT HUP INT TERM
  # Restore clocks before slower cleanup so an external KILL cannot leave fixed CPU state behind.
  restore_cpu_policy
  safe_exit_retroarch || true
  signal_pid_quick "$RA_PID"
  signal_pid_quick "$JOY_PID"
  stop_joystickd
  if [ -n "$RA_PID" ] && kill -0 "$RA_PID" 2>/dev/null; then
    wait "$RA_PID" 2>/dev/null || true
  fi
  rm -f "$APPEND"
  exit "$rc"
}

run_retroarch() {
  if [ -n "$GAMEPAD_JS" ]; then
    SDL_JOYSTICK_DEVICE="$GAMEPAD_JS" \
    PLUMOS_RA_DISPLAY_ROTATION="$ROTATION" \
    PLUMOS_RA_LANDSCAPE_MODE="$LANDSCAPE_MODE" \
      "$RA" --appendconfig "$APPEND" ${MAX_FRAMES:+"--max-frames=$MAX_FRAMES"} ${ENTRY_SLOT:+"--entryslot=$ENTRY_SLOT"} -L "$CORE" "$ROM" >"$RA_LOG" 2>&1 &
  else
    PLUMOS_RA_DISPLAY_ROTATION="$ROTATION" \
    PLUMOS_RA_LANDSCAPE_MODE="$LANDSCAPE_MODE" \
      "$RA" --appendconfig "$APPEND" ${MAX_FRAMES:+"--max-frames=$MAX_FRAMES"} ${ENTRY_SLOT:+"--entryslot=$ENTRY_SLOT"} -L "$CORE" "$ROM" >"$RA_LOG" 2>&1 &
  fi
  RA_PID=$!
  if wait "$RA_PID"; then
    rc=0
  else
    rc=$?
  fi
  RA_PID=""
  exit "$rc"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --core)
      CORE="${2:?missing core path}"
      shift
      ;;
    --rom)
      ROM="${2:?missing ROM path}"
      shift
      ;;
    --system)
      SYSTEM="${2:?missing system}"
      shift
      ;;
    --audio)
      AUDIO="${2:?missing audio driver}"
      shift
      ;;
    --audio-latency)
      AUDIO_LATENCY="${2:?missing audio latency}"
      shift
      ;;
    --cpu)
      CPU_POLICY="${2:?missing CPU policy}"
      shift
      ;;
    --freq)
      CPU_FREQ="${2:?missing CPU frequency}"
      shift
      ;;
    --cores)
      CPU_CORES="${2:?missing CPU core count}"
      shift
      ;;
    --rotation)
      ROTATION="${2:?missing rotation}"
      shift
      ;;
    --landscape-mode)
      LANDSCAPE_MODE="${2:?missing landscape mode}"
      shift
      ;;
    --vrr-runloop)
      VRR_RUNLOOP="${2:?missing vrr runloop value}"
      shift
      ;;
    --max-frames)
      MAX_FRAMES="${2:?missing max frames}"
      shift
      ;;
    --quit-press-twice)
      QUIT_PRESS_TWICE="${2:?missing quit press twice value}"
      shift
      ;;
    --entry-slot)
      ENTRY_SLOT="${2:?missing entry slot value}"
      shift
      ;;
    --safe-state-slot)
      SAFE_STATE_SLOT="${2:?missing safe state slot value}"
      shift
      ;;
    --safe-exit)
      SAFE_EXIT="${2:?missing safe exit value}"
      shift
      ;;
    --dosbox-pure-force60fps)
      DOSBOX_PURE_FORCE60FPS="${2:?missing DOSBox-Pure force60fps value}"
      shift
      ;;
    --dosbox-pure-cycles)
      DOSBOX_PURE_CYCLES="${2:?missing DOSBox-Pure cycles value}"
      shift
      ;;
    --no-joystickd)
      USE_JOYSTICKD=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

case "$AUDIO" in
  oss|alsa) ;;
  *) echo "error: invalid audio driver: $AUDIO" >&2; exit 2 ;;
esac
case "$AUDIO_LATENCY" in
  ''|*[!0-9]*) [ -z "$AUDIO_LATENCY" ] || { echo "error: invalid audio latency: $AUDIO_LATENCY" >&2; exit 2; } ;;
esac
case "$CPU_POLICY" in
  performance|fixed) ;;
  *) echo "error: invalid CPU policy: $CPU_POLICY" >&2; exit 2 ;;
esac
case "$CPU_CORES" in
  2|4) ;;
  *) echo "error: invalid CPU cores: $CPU_CORES" >&2; exit 2 ;;
esac
case "$CPU_FREQ" in
  ''|*[!0-9]*) echo "error: invalid CPU frequency: $CPU_FREQ" >&2; exit 2 ;;
esac
case "$MAX_FRAMES" in
  ''|*[!0-9]*) [ -z "$MAX_FRAMES" ] || { echo "error: invalid max frames: $MAX_FRAMES" >&2; exit 2; } ;;
esac
case "$QUIT_PRESS_TWICE" in
  ''|true|false) ;;
  *) echo "error: invalid quit_press_twice: $QUIT_PRESS_TWICE" >&2; exit 2 ;;
esac
case "$VRR_RUNLOOP" in
  true|false) ;;
  *) echo "error: invalid vrr_runloop: $VRR_RUNLOOP" >&2; exit 2 ;;
esac
case "$ENTRY_SLOT" in
  '') ;;
  *) validate_state_slot "entry slot" "$ENTRY_SLOT" ;;
esac
validate_state_slot "safe state slot" "$SAFE_STATE_SLOT"
case "$SAFE_EXIT" in
  0|1|yes|no|true|false|on|off) ;;
  *) echo "error: invalid safe exit: $SAFE_EXIT" >&2; exit 2 ;;
esac
case "$SAFE_EXIT_SAVE_STATE" in
  0|1|yes|no|true|false|on|off) ;;
  *) echo "error: invalid safe exit save state: $SAFE_EXIT_SAVE_STATE" >&2; exit 2 ;;
esac
case "$SAFE_EXIT_SAVE_FILES" in
  0|1|yes|no|true|false|on|off) ;;
  *) echo "error: invalid safe exit save files: $SAFE_EXIT_SAVE_FILES" >&2; exit 2 ;;
esac
case "$SAFE_EXIT_CLOSE_CONTENT" in
  0|1|yes|no|true|false|on|off) ;;
  *) echo "error: invalid safe exit close content: $SAFE_EXIT_CLOSE_CONTENT" >&2; exit 2 ;;
esac
case "$DOSBOX_PURE_FORCE60FPS" in
  ''|true|false) ;;
  *) echo "error: invalid DOSBox-Pure force60fps: $DOSBOX_PURE_FORCE60FPS" >&2; exit 2 ;;
esac
case "$DOSBOX_PURE_CYCLES" in
  ''|auto|max|[1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-9][0-9][0-9][0-9]|[1-9][0-9][0-9][0-9][0-9]|[1-9][0-9][0-9][0-9][0-9][0-9]|1000000) ;;
  *) echo "error: invalid DOSBox-Pure cycles: $DOSBOX_PURE_CYCLES" >&2; exit 2 ;;
esac

case "$SYSTEM" in
  "")
    ;;
  nes)
    CORE="${CORE:-$PLUMOS_ROOT/retroarch/cores/fceumm_libretro.so}"
    ROM="${ROM:-$(first_rom /mnt/SDCARD/Roms/FC "*.nes")}"
    ;;
  gb)
    CORE="${CORE:-$PLUMOS_ROOT/retroarch/cores/gambatte_libretro.so}"
    ROM="${ROM:-$(first_rom /mnt/SDCARD/Roms/GB "*.gb")}"
    ;;
  *)
    if [ -z "$CORE" ] || [ -z "$ROM" ]; then
      echo "error: unknown system without explicit --core and --rom: $SYSTEM" >&2
      exit 2
    fi
    ;;
esac

[ -n "$CORE" ] || { echo "error: --core or --system is required" >&2; exit 2; }
[ -n "$ROM" ] || { echo "error: --rom is required or no ROM was found for --system" >&2; exit 2; }
[ -x "$RA" ] || { echo "error: RetroArch wrapper not found: $RA" >&2; exit 1; }

mkdir -p "$LOG_DIR"
trap cleanup EXIT INT TERM
apply_system_volume

cat > "$APPEND" <<APPEND
config_save_on_exit = "false"
state_slot = "$SAFE_STATE_SLOT"
vrr_runloop_enable = "$VRR_RUNLOOP"
APPEND
if [ -n "$QUIT_PRESS_TWICE" ]; then
  cat >> "$APPEND" <<APPEND
quit_press_twice = "$QUIT_PRESS_TWICE"
APPEND
fi

if [ "$AUDIO" = "alsa" ]; then
  cat >> "$APPEND" <<APPEND
audio_driver = "alsa"
audio_device = "default"
APPEND
elif [ "$AUDIO" = "oss" ]; then
  SYSTEM_VOLUME="$(read_system_volume)"
  RA_AUDIO_VOLUME="$(retroarch_audio_volume_db "$SYSTEM_VOLUME")"
  if [ "$SYSTEM_VOLUME" -le 0 ]; then
    RA_AUDIO_MUTE=true
  else
    RA_AUDIO_MUTE=false
  fi
  cat >> "$APPEND" <<APPEND
audio_driver = "oss"
audio_device = "/dev/dsp"
audio_volume = "$RA_AUDIO_VOLUME"
audio_mute_enable = "$RA_AUDIO_MUTE"
APPEND
fi
if [ -n "$AUDIO_LATENCY" ]; then
  cat >> "$APPEND" <<APPEND
audio_latency = "$AUDIO_LATENCY"
APPEND
fi
if [ -n "$DOSBOX_PURE_FORCE60FPS" ]; then
  cat >> "$APPEND" <<APPEND
dosbox_pure_force60fps = "$DOSBOX_PURE_FORCE60FPS"
APPEND
fi
if [ -n "$DOSBOX_PURE_CYCLES" ]; then
  cat >> "$APPEND" <<APPEND
dosbox_pure_cycles = "$DOSBOX_PURE_CYCLES"
APPEND
fi

apply_cpu_policy
ensure_loopback_up

if [ "$USE_JOYSTICKD" = "1" ] && [ -x "$JOY" ]; then
  "$JOY" --device-mode xbox >"$JOY_LOG" 2>&1 &
  JOY_PID=$!
  wait_for_gamepad_js || true
fi

run_retroarch
EOF
  chmod +x "${TARGET_DIR}/plumos/bin/plumos-retroarch-launch"

  cp "${SRC_DIR}/COPYING" "${TARGET_DIR}/docs/RetroArch-COPYING"

  {
    printf 'RetroArch practical A30 runtime build\n'
    printf 'version=%s\n' "${RETROARCH_VERSION}"
    printf 'tag=%s\n' "${RETROARCH_TAG}"
    printf 'source=%s\n' "${RETROARCH_URL}"
    printf 'sha256=%s\n' "${RETROARCH_SHA256}"
    printf 'configure=GLESv2/EGL Mali fbdev RGUI; OSS/ALSA audio; SDL2/udev/linuxraw input; screenshots/images; zlib/7zip/network command\n'
    printf 'patches=A30 GL2 display rotation; A30 landscape FBO present; A30 restart exec path; NCI SAVE_STATE_SLOT command\n'
    printf 'launcher=plumos-retroarch-launch defaults to ALSA, SDL2 joypad, fixed 648000 kHz CPU policy, 2 CPU cores, safe state slot 999, and vrr_runloop_enable=true\n'
    printf '\nNEEDED:\n'
    "${READELF}" -d "${TARGET_DIR}/plumos/retroarch/bin/retroarch.bin" | awk '/NEEDED/ {print}'
  } > "${TARGET_DIR}/docs/manifest.txt"

  msg "staged ${TARGET_DIR}"
}

fetch_archive
ensure_sdl2_runtime
prepare_source
apply_patches
build_retroarch
stage_dist
