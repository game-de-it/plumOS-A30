#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-gmu"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/gmu/src"}
DEPS_ROOT=${DEPS_ROOT:-"${BUILD_ROOT}/gmu/deps"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

GMU_REPO=${GMU_REPO:-https://github.com/denis-n-kuznetsov/gmu.git}
GMU_REF=${GMU_REF:-d0ef466fa52297a7f6e92db1c39d0c59d3158740}
GMU_PLUMOS_PATCH=${GMU_PLUMOS_PATCH:-"${ROOT_DIR}/docker/plumos-toolchain/patches/gmu-d0ef466-plumos-a30.patch"}
SDL_IMAGE_VERSION=${SDL_IMAGE_VERSION:-1.2.12}
SDL_IMAGE_URL=${SDL_IMAGE_URL:-https://www.libsdl.org/projects/SDL_image/release/SDL_image-${SDL_IMAGE_VERSION}.tar.gz}

CROSS_PREFIX=${CROSS_PREFIX:-arm-linux-gnueabihf-}
CC=${CC:-${CROSS_PREFIX}gcc}
CXX=${CXX:-${CROSS_PREFIX}g++}
STRIP=${STRIP:-${CROSS_PREFIX}strip}
READELF=${READELF:-${CROSS_PREFIX}readelf}

msg() {
  printf '[gmu] %s\n' "$*" >&2
}

tool_path() {
  command -v "$1" 2>/dev/null || printf '%s\n' "$1"
}

find_target_lib() {
  local name=$1
  local dir
  for dir in \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf/pulseaudio \
    /usr/arm-linux-gnueabihf/lib \
    /lib \
    /usr/lib; do
    if [ -e "${dir}/${name}" ]; then
      printf '%s/%s\n' "${dir}" "${name}"
      return 0
    fi
  done
  return 1
}

copy_if_present() {
  local src=$1
  local dst=$2
  local soname=${3:-$(basename "${src}")}
  local real_src
  local real_base

  [ -e "${src}" ] || return 0
  mkdir -p "${dst}"
  real_src=$(readlink -f "${src}")
  real_base=$(basename "${real_src}")
  if [ ! -f "${dst}/${real_base}" ]; then
    install -m 0644 "${real_src}" "${dst}/${real_base}"
  fi
  if [ "${soname}" != "${real_base}" ] && [ ! -f "${dst}/${soname}" ]; then
    cp -f "${dst}/${real_base}" "${dst}/${soname}"
    chmod 0644 "${dst}/${soname}"
  fi
}

copy_dep_tree() {
  local elf=$1
  local lib_dir=$2
  local dep
  local path

  [ -f "${elf}" ] || return 0
  "${READELF}" -d "${elf}" 2>/dev/null |
    awk -F"[][]" '/NEEDED/ {print $2}' |
    while IFS= read -r dep; do
      [ -n "${dep}" ] || continue
      case "${dep}" in
        ld-linux-armhf.so.3|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|libgcc_s.so.1)
          continue
          ;;
      esac
      if [ "${dep}" = "libSDL_image-1.2.so.0" ]; then
        path="${SDL_IMAGE_PREFIX}/lib/${dep}"
      else
        path=$(find_target_lib "${dep}" || true)
      fi
      if [ -z "${path}" ] || [ ! -e "${path}" ]; then
        msg "warning: could not locate runtime dependency ${dep}"
        continue
      fi
      if [ ! -f "${lib_dir}/${dep}" ]; then
        copy_if_present "${path}" "${lib_dir}" "${dep}"
        copy_dep_tree "$(readlink -f "${path}")" "${lib_dir}"
      fi
    done
}

clone_gmu() {
  local src=$1

  rm -rf "${src}"
  mkdir -p "$(dirname "${src}")"
  msg "cloning ${GMU_REPO} (${GMU_REF})"
  git clone --depth 1 "${GMU_REPO}" "${src}"
  git -C "${src}" fetch --depth 1 origin "${GMU_REF}"
  git -C "${src}" checkout --detach FETCH_HEAD
  if [ -f "${GMU_PLUMOS_PATCH}" ]; then
    msg "applying plumOS A30 patch"
    git -C "${src}" apply "${GMU_PLUMOS_PATCH}"
  fi
}

build_sdl_image() {
  local build_dir="${DEPS_ROOT}/SDL_image-${SDL_IMAGE_VERSION}"
  local tarball="${DEPS_ROOT}/SDL_image-${SDL_IMAGE_VERSION}.tar.gz"

  if [ -f "${SDL_IMAGE_PREFIX}/lib/libSDL_image-1.2.so.0" ] &&
     [ -f "${SDL_IMAGE_PREFIX}/include/SDL/SDL_image.h" ]; then
    msg "using cached SDL_image ${SDL_IMAGE_VERSION}"
    return 0
  fi

  rm -rf "${build_dir}" "${SDL_IMAGE_PREFIX}"
  mkdir -p "${DEPS_ROOT}" "${SDL_IMAGE_PREFIX}"
  if [ ! -f "${tarball}" ]; then
    msg "downloading SDL_image ${SDL_IMAGE_VERSION}"
    wget -q -O "${tarball}" "${SDL_IMAGE_URL}"
  fi
  tar -xzf "${tarball}" -C "${DEPS_ROOT}"
  msg "building SDL_image ${SDL_IMAGE_VERSION}"
  (
    cd "${build_dir}"
    CC="$(tool_path "${CC}")" \
      ./configure \
        --host=arm-linux-gnueabihf \
        --prefix="${SDL_IMAGE_PREFIX}" \
        --disable-sdltest \
        --disable-jpg-shared \
        --disable-png-shared \
        --disable-tif \
        --disable-webp
    make -j"${JOBS}"
    make install
  )
}

write_gmu_config() {
  local out=$1
  cat >"${out}" <<'EOF'
Gmu.AutoPlayOnProgramStart=no
Gmu.DefaultFileBrowserPath=/mnt/SDCARD/Music
Gmu.DefaultPlayMode=continue
Gmu.DeviceCloseASAP=yes
Gmu.FadeOutOnSkip=no
Gmu.FileBrowserFoldersFirst=yes
Gmu.FileSystemCharset=UTF-8
Gmu.FirstRun=no
Gmu.LastPlayedPlaylistItem=None
Gmu.LastPlayedPlaylistItemTime=0
Gmu.ReaderCache=512
Gmu.ReaderCachePrebufferSize=256
Gmu.RememberLastPlaylist=yes
Gmu.RememberSettings=yes
Gmu.ResumePlayback=yes
Gmu.Shutdown=0
Gmu.ShutdownCommand=/sbin/poweroff
Gmu.Volume=70
Gmu.VolumeControl=Software
Gmu.VolumeHardwareMixerChannel=0
Log.Enable=no
Log.MinimumPlaytimeSec=30
Log.MinimumPlaytimePercent=50
SDL.AllowVolumeControlInHoldState=no
SDL.AutoSelectCurrentPlaylistItem=yes
SDL.BacklightPowerOnOnTrackChange=no
SDL.CoverArtworkFilePattern=cover.jpg;cover.png;front.jpg;*.jpg;*.png
SDL.CoverArtworkLarge=no
SDL.DefaultSkin=default-modern-large
SDL.EnableCoverArtwork=no
SDL.FileBrowserSelectNextAfterAdd=yes
SDL.Fullscreen=no
SDL.Width=320
SDL.Height=240
SDL.InputConfigFile=gmuinput.a30.conf
SDL.KeyMap=a30.keymap
SDL.LoadEmbeddedCoverArtwork=first
SDL.LyricsFilePattern=$.txt;*.txt;*.nfo
SDL.Scroll=always
SDL.SecondsUntilBacklightPowerOff=30
SDL.SmallCoverArtworkAlignment=right
SDL.TimeDisplay=elapsed
EOF
}

write_input_config() {
  local out=$1
  cat >"${out}" <<'EOF'
FullKeyboard=no
JoyAxis-0=-2,Up
JoyAxis-1=2,Down
JoyAxis-2=-1,Left
JoyAxis-3=1,Right
JoyButton-0=8,Select
JoyButton-1=9,Start
JoyButton-2=0,A
JoyButton-3=1,B
JoyButton-4=2,X
JoyButton-5=3,Y
JoyButton-6=5,R
JoyButton-7=4,L
JoyButton-8=10,Menu
Button-0=273,Up
Button-1=274,Down
Button-2=276,Left
Button-3=275,Right
Button-4=27,Select
Button-5=13,Start
Button-6=306,A
Button-7=308,B
Button-8=304,X
Button-9=32,Y
Button-10=8,R
Button-11=9,L
Button-12=27,Menu
EOF
}

write_keymap() {
  local out=$1
  cat >"${out}" <<'EOF'
Modifier=Select

Up=Up
Down=Down
IncreaseVolume=Right
DecreaseVolume=Left
ToggleTime=Mod+Left
ShutdownTimer=Mod+Down
Pause=A
Stop=Mod+A
ToggleView=Start
PreviousTrack=L
NextTrack=R
SeekForward=Mod+R
SeekBackward=Mod+L
Exit=Menu
Help=Mod+Up

ProgramInfo=Y
ProgramInfoOkay=A

FileBrowserPlayFile=A
FileBrowserAddFileToPlaylistOrChDir=A
FileBrowserAddDirToPlaylist=X
FileBrowserInsertFileIntoPlaylist=Mod+A
FileBrowserNewPlFromDir=Mod+X

PlaylistPlayItem=A
PlaylistToggleRandomMode=B
PlaylistRemoveItem=X
PlaylistClear=Mod+X
PlaylistSave=Mod+B

PlaylistSaveSelect=A
PlaylistSaveCancel=B
PlaylistSaveLoadList=X
PlaylistSaveAppendList=Mod+X
PlaylistQueue=Mod+Right

TrackInfoToggleCover=A
TrackInfoToggleText=B

QuestionYes=A
QuestionNo=B

SetupSelect=A
SetupSaveAndExit=Y
SetupSaveAndRunGmu=X
SetupFileBrowserSelect=A
SetupFileBrowserChDir=A
SetupFileBrowserCancel=B
EOF
}

write_launcher() {
  local out=$1
  cat >"${out}" <<'EOF'
#!/bin/sh
set -u

PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
APP_ROOT="${PLUMOS_GMU_ROOT:-${PLUMOS_ROOT}/apps/gmu}"
STATE_DIR="${PLUMOS_GMU_STATE:-${PLUMOS_ROOT}/state/apps/gmu}"
LOG_DIR="${PLUMOS_GMU_LOG_DIR:-${PLUMOS_ROOT}/logs/apps}"
LOADER="${PLUMOS_ROOT}/lib/ld-linux-armhf.so.3"
JOYSTICKD_OPTS="${PLUMOS_GMU_JOYSTICKD_OPTS:---device-mode xbox --trigger-mode buttons --shoulder-layout user --function-button guide}"
joystickd_pid=
cleanup_done=0

mkdir -p "${STATE_DIR}/.config" "${STATE_DIR}/playlists" "${LOG_DIR}" /mnt/SDCARD/Music 2>/dev/null || true

cleanup() {
  [ "${cleanup_done}" = 1 ] && return
  cleanup_done=1
  if [ -n "${joystickd_pid:-}" ] && kill -0 "${joystickd_pid}" >/dev/null 2>&1; then
    kill -TERM "${joystickd_pid}" >/dev/null 2>&1 || true
    sleep 1
    kill -KILL "${joystickd_pid}" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM HUP

if [ ! -x "${APP_ROOT}/bin/gmu.bin" ]; then
  echo "error: missing GMU binary: ${APP_ROOT}/bin/gmu.bin" >&2
  exit 127
fi
if [ ! -x "${LOADER}" ]; then
  echo "error: missing dynamic loader: ${LOADER}" >&2
  exit 127
fi

if [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ]; then
  "${PLUMOS_ROOT}/bin/plumos-joystickd" ${JOYSTICKD_OPTS} \
    >"${LOG_DIR}/gmu-joystickd.log" 2>&1 &
  joystickd_pid=$!
  sleep 1
fi

export HOME="${STATE_DIR}"
export XDG_CONFIG_HOME="${STATE_DIR}/.config"
export PLUMOS_GMU_A30_PRESENT="${PLUMOS_GMU_A30_PRESENT:-${PLUMOS_GMU_A30_MALI:-1}}"
export PLUMOS_GMU_A30_ROTATION="${PLUMOS_GMU_A30_ROTATION:-ccw}"
export PLUMOS_GMU_A30_EGL="${PLUMOS_GMU_A30_EGL:-0}"
export PLUMOS_GMU_A30_VSYNC="${PLUMOS_GMU_A30_VSYNC:-1}"
export PLUMOS_GMU_A30_LINEAR="${PLUMOS_GMU_A30_LINEAR:-0}"
case "${PLUMOS_GMU_A30_PRESENT}" in
  0|no|false|off|OFF|False|FALSE)
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-fbcon}"
    ;;
  *)
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
    ;;
esac
export SDL_FBDEV="${SDL_FBDEV:-/dev/fb0}"
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_AUDIO_ALSA_SET_BUFFER_SIZE="${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}"
export PATH="${PLUMOS_ROOT}/bin:${PLUMOS_ROOT}/gnu/bin:/usr/sbin:/usr/bin:/sbin:/bin"

LIB_PATH="${APP_ROOT}/lib:${PLUMOS_ROOT}/lib:/usr/lib:/lib"
cd "${APP_ROOT}" || exit 1

"${LOADER}" \
  --library-path "${LIB_PATH}" \
  "${APP_ROOT}/bin/gmu.bin" \
  -d "${APP_ROOT}/config" -c gmu.a30.conf
rc=$?
sync
exit "${rc}"
EOF
  chmod 0755 "${out}"
}

main() {
  local src="${SRC_ROOT}/gmu"
  local sdl_image_prefix="${DEPS_ROOT}/SDL_image-prefix"
  local app_dir="${TARGET_DIR}/plumos/apps/gmu"
  local bin_dir="${app_dir}/bin"
  local lib_dir="${app_dir}/lib"
  local config_dir="${app_dir}/config"
  local doc_dir="${TARGET_DIR}/plumos/share/doc/gmu"
  local log_dir="${TARGET_DIR}/docs/build-logs"
  local build_log="${log_dir}/gmu.log"

  export SDL_IMAGE_PREFIX="${sdl_image_prefix}"

  rm -rf "${TARGET_DIR}"
  mkdir -p "${bin_dir}" "${lib_dir}" "${config_dir}" "${doc_dir}" "${log_dir}" \
    "${TARGET_DIR}/plumos/bin"

  build_sdl_image >"${build_log}" 2>&1
  clone_gmu "${src}" >>"${build_log}" 2>&1

  cat >"${src}/config.mk" <<EOF
TARGET=unknown
DECODERS_TO_BUILD=decoders/mpg123.so decoders/flac.so
FRONTENDS_TO_BUILD=frontends/sdl.so
TOOLS_TO_BUILD=\$(BINARY)
CC=$(tool_path "${CC}")
CXX=$(tool_path "${CXX}")
STRIP=$(tool_path "${STRIP}")
SDL_LIB=\$(shell sdl-config --libs)
SDL_CFLAGS=\$(shell sdl-config --cflags)
COPTS=-O2 -pipe -marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fomit-frame-pointer -ffunction-sections -fdata-sections -ffast-math -fcommon
CFLAGS=\$(SDL_CFLAGS) -I${sdl_image_prefix}/include -I${sdl_image_prefix}/include/SDL -fsigned-char -D_REENTRANT -DUSE_MEMORY_H -DNDEBUG
LFLAGS=-L${sdl_image_prefix}/lib -Wl,-rpath-link,${sdl_image_prefix}/lib -Wl,-export-dynamic -Wl,--gc-sections -lpthread -lm -ldl -lz
SDLFE_WITHOUT_SDL_GFX=1
DISTFILES=\$(COMMON_DISTBIN_FILES) gmuinput.unknown.conf gmu.sh
EOF

  msg "building GMU"
  make -C "${src}" clean >>"${build_log}" 2>&1 || true
  make -C "${src}" -j"${JOBS}" V=1 >>"${build_log}" 2>&1

  install -m 0755 "${src}/gmu.bin" "${bin_dir}/gmu.bin"
  "${STRIP}" "${bin_dir}/gmu.bin" 2>/dev/null || true
  mkdir -p "${app_dir}/frontends" "${app_dir}/decoders" "${app_dir}/themes"
  install -m 0755 "${src}/frontends/sdl.so" "${app_dir}/frontends/sdl.so"
  install -m 0755 "${src}/decoders/mpg123.so" "${app_dir}/decoders/mpg123.so"
  install -m 0755 "${src}/decoders/flac.so" "${app_dir}/decoders/flac.so"
  "${STRIP}" "${app_dir}/frontends/sdl.so" "${app_dir}/decoders/"*.so 2>/dev/null || true
  cp -a "${src}/themes/default-modern-large" "${app_dir}/themes/"
  cp -a "${src}/themes/default-modern" "${app_dir}/themes/"
  install -m 0644 "${src}/gmu.png" "${app_dir}/gmu.png"
  install -m 0644 "${src}/COPYING" "${doc_dir}/COPYING"
  install -m 0644 "${src}/README.txt" "${doc_dir}/README.txt"
  install -m 0644 "${src}/BUILD.txt" "${doc_dir}/BUILD.txt"

  write_gmu_config "${config_dir}/gmu.a30.conf"
  write_input_config "${config_dir}/gmuinput.a30.conf"
  write_keymap "${config_dir}/a30.keymap"
  write_launcher "${TARGET_DIR}/plumos/bin/plumos-gmu-launch"

  copy_if_present "${sdl_image_prefix}/lib/libSDL_image-1.2.so.0" "${lib_dir}" "libSDL_image-1.2.so.0"
  for elf in \
    "${bin_dir}/gmu.bin" \
    "${app_dir}/frontends/sdl.so" \
    "${app_dir}/decoders/mpg123.so" \
    "${app_dir}/decoders/flac.so"; do
    copy_dep_tree "${elf}" "${lib_dir}"
  done

  {
    printf 'GMU for plumOS A30\n'
    printf 'repo=%s\n' "${GMU_REPO}"
    printf 'ref=%s\n' "${GMU_REF}"
    printf 'sdl_image_version=%s\n' "${SDL_IMAGE_VERSION}"
    printf 'binary=plumos/apps/gmu/bin/gmu.bin\n'
    printf 'launcher=plumos/bin/plumos-gmu-launch\n'
    printf 'theme=default-modern-large\n'
    printf 'decoders=mpg123 flac\n'
    printf 'a30_presenter=fbdev-direct; set PLUMOS_GMU_A30_EGL=1 for experimental EGL path\n'
    printf 'license=GPL-2.0; see COPYING\n'
    printf 'note=GMU SDL frontend uses bitmap fonts; CJK filenames are not fully renderable without a font renderer patch.\n'
    printf '\n[needed]\n'
    for elf in \
      "${bin_dir}/gmu.bin" \
      "${app_dir}/frontends/sdl.so" \
      "${app_dir}/decoders/mpg123.so" \
      "${app_dir}/decoders/flac.so"; do
      printf '%s\n' "$(basename "${elf}")"
      "${READELF}" -d "${elf}" 2>/dev/null | awk -F"[][]" '/NEEDED/ {print "  " $2}' || true
    done
  } >"${doc_dir}/manifest.txt"
  cp -f "${build_log}" "${doc_dir}/build.log"

  msg "staged ${TARGET_DIR}"
}

main "$@"
