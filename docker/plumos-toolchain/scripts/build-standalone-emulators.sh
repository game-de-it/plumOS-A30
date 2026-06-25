#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR=${ROOT_DIR:-/workspace}
BUILD_ROOT=${BUILD_ROOT:-"${ROOT_DIR}/build"}
DIST_ROOT=${DIST_ROOT:-"${ROOT_DIR}/dist"}
TARGET_DIR=${TARGET_DIR:-"${DIST_ROOT}/plumos-standalone-emulators"}
SRC_ROOT=${SRC_ROOT:-"${BUILD_ROOT}/standalone-emulators/src"}
REF_ROOT=${REF_ROOT:-"${BUILD_ROOT}/standalone-emulators/inspect"}
PATCH_DIR=${PATCH_DIR:-"${ROOT_DIR}/docker/plumos-toolchain/patches"}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

PLUMOS_STANDALONE_FILTER=${PLUMOS_STANDALONE_FILTER:-all}
FAIL_ON_STANDALONE_ERROR=${FAIL_ON_STANDALONE_ERROR:-0}
PLUMOS_STANDALONE_INCLUDE_EXPERIMENTAL=${PLUMOS_STANDALONE_INCLUDE_EXPERIMENTAL:-0}

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

PPSSPP_REPO=${PPSSPP_REPO:-https://github.com/hrydgard/ppsspp.git}
PPSSPP_REF=${PPSSPP_REF:-v1.20.4}
PPSSPP_PATCH_MODE=${PPSSPP_PATCH_MODE:-a30}
PPSSPP_STAGE_ID=${PPSSPP_STAGE_ID:-ppsspp}
PPSSPP_BINARY_NAME=${PPSSPP_BINARY_NAME:-PPSSPPSDL}
PPSSPP_USE_FFMPEG=${PPSSPP_USE_FFMPEG:-ON}
SCUMMVM_REPO=${SCUMMVM_REPO:-https://github.com/scummvm/scummvm.git}
SCUMMVM_REF=${SCUMMVM_REF:-v2026.2.0}
EASYRPG_REPO=${EASYRPG_REPO:-https://github.com/EasyRPG/Player.git}
EASYRPG_REF=${EASYRPG_REF:-0.8.1.1}
DOSBOX_REPO=${DOSBOX_REPO:-https://github.com/dosbox-staging/dosbox-staging.git}
DOSBOX_REF=${DOSBOX_REF:-v0.82.2}
PCSX_REARMED_REPO=${PCSX_REARMED_REPO:-https://github.com/notaz/pcsx_rearmed.git}
PCSX_REARMED_REF=${PCSX_REARMED_REF:-r26l}
RED_VIPER_REPO=${RED_VIPER_REPO:-https://github.com/skyfloogle/red-viper.git}
RED_VIPER_REF=${RED_VIPER_REF:-afb8080ef38f63e067099505d2b8c608b1766b30}
RED_VIPER_A30_FRONTEND_DIR=${RED_VIPER_A30_FRONTEND_DIR:-"${ROOT_DIR}/docker/plumos-toolchain/red-viper-a30"}
OPENBOR_REPO=${OPENBOR_REPO:-https://github.com/DCurrent/openbor.git}
OPENBOR_REF=${OPENBOR_REF:-v6391}

SCUMMVM_ENGINES=${SCUMMVM_ENGINES:-"scumm,agi,agos,sky,sword1,sword2,queen,gob,lure,kyra,sci,cine,drascula,touche,teenagent,tinsel,cruise,parallaction"}

MANIFEST=""
LOG_DIR=""
BUILT_COUNT=0
FAILED_COUNT=0
SKIPPED_COUNT=0

msg() {
  printf '[standalone-emulators] %s\n' "$*" >&2
}

append_manifest() {
  printf '%s\n' "$*" >>"${MANIFEST}"
}

tool_path() {
  command -v "$1" 2>/dev/null || printf '%s\n' "$1"
}

meson_word_array() {
  local words=$1
  local first=1
  local word
  for word in ${words}; do
    if [ "${first}" -eq 0 ]; then
      printf ', '
    fi
    printf "'%s'" "${word}"
    first=0
  done
}

copy_if_present() {
  local src=$1
  local dst=$2
  local soname=${3:-$(basename "${src}")}
  local real_src real_base

  if [ ! -e "${src}" ]; then
    return 0
  fi

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

copy_dep_tree() {
  local elf=$1
  local lib_dir=$2
  local dep path

  if [ ! -f "${elf}" ]; then
    return 0
  fi

  "${READELF}" -d "${elf}" 2>/dev/null |
    awk -F"[][]" '/NEEDED/ {print $2}' |
    while IFS= read -r dep; do
      [ -n "${dep}" ] || continue
      case "${dep}" in
        libEGL.so*|libGLESv2.so*|libMali.so*|libGL.so*)
          continue
          ;;
      esac
      path=$(find_target_lib "${dep}" || true)
      if [ -z "${path}" ]; then
        msg "warning: could not locate runtime dependency ${dep}"
        continue
      fi
      if [ ! -f "${lib_dir}/${dep}" ]; then
        copy_if_present "${path}" "${lib_dir}" "${dep}"
        copy_dep_tree "$(readlink -f "${path}")" "${lib_dir}"
      fi
    done
}

copy_runtime_deps() {
  local binary=$1
  local lib_dir=$2
  local lib path
  mkdir -p "${lib_dir}"

  for lib in ld-linux-armhf.so.3 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 \
    librt.so.1 libgcc_s.so.1 libstdc++.so.6; do
    path=$(find_target_lib "${lib}" || true)
    if [ -n "${path}" ]; then
      copy_if_present "${path}" "${lib_dir}" "${lib}"
    fi
  done

  copy_dep_tree "${binary}" "${lib_dir}"
}

emulator_selected() {
  local id=$1
  local filter=",$PLUMOS_STANDALONE_FILTER,"
  case "${PLUMOS_STANDALONE_FILTER}" in
    all|ALL) return 0 ;;
  esac
  case "${filter}" in
    *,"${id}",*) return 0 ;;
    *) return 1 ;;
  esac
}

red_viper_build_enabled() {
  local filter=",$PLUMOS_STANDALONE_FILTER,"
  case "${PLUMOS_STANDALONE_FILTER}" in
    all|ALL)
      [ "${PLUMOS_STANDALONE_INCLUDE_EXPERIMENTAL}" = "1" ]
      return
      ;;
  esac
  case "${filter}" in
    *,red_viper,*|*,red-viper,*) return 0 ;;
    *) return 1 ;;
  esac
}

clone_repo() {
  local id=$1
  local repo=$2
  local ref=$3
  local dst="${SRC_ROOT}/${id}"
  local log=$4
  local ref_opt=()
  local recurse_opt=(--recursive --shallow-submodules)

  rm -rf "${dst}"
  mkdir -p "$(dirname "${dst}")"

  case "${id}" in
    red_viper)
      recurse_opt=(--no-recurse-submodules)
      ;;
  esac

  case "${ref}" in
    HEAD)
      ;;
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*)
      ;;
    *)
      ref_opt=(--branch "${ref}")
      ;;
  esac

  if [ "${ref}" != "HEAD" ] && [ "${#ref}" -lt 12 ]; then
    ref_opt=(--branch "${ref}")
  fi

  msg "cloning ${id} (${ref})"
  git clone \
    --depth 1 \
    --reference-if-able "${REF_ROOT}/${id}" \
    "${recurse_opt[@]}" \
    "${ref_opt[@]}" \
    "${repo}" \
    "${dst}" >>"${log}" 2>&1 || return 1

  case "${ref}" in
    HEAD)
      ;;
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*)
      git -C "${dst}" fetch --depth 1 origin "${ref}" >>"${log}" 2>&1 || return 1
      git -C "${dst}" checkout --detach FETCH_HEAD >>"${log}" 2>&1 || return 1
      ;;
  esac
}

create_cmake_toolchain() {
  local out=$1
  cat >"${out}" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER $(tool_path "${CC}"))
set(CMAKE_CXX_COMPILER $(tool_path "${CXX}"))
set(CMAKE_AR $(tool_path "${AR}"))
set(CMAKE_RANLIB $(tool_path "${RANLIB}"))
set(CMAKE_STRIP $(tool_path "${STRIP}"))
set(CMAKE_FIND_ROOT_PATH /usr /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
}

run_cmake() {
  local src=$1
  local build_dir=$2
  shift 2
  local toolchain="${build_dir}/plumos-armhf-toolchain.cmake"

  rm -rf "${build_dir}"
  mkdir -p "${build_dir}"
  create_cmake_toolchain "${toolchain}"

  PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig \
  PKG_CONFIG_ALLOW_CROSS=1 \
  cmake -S "${src}" -B "${build_dir}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="${COMMON_CFLAGS}" \
    -DCMAKE_CXX_FLAGS="${COMMON_CXXFLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="${COMMON_LDFLAGS}" \
    "$@"
}

stage_binary() {
  local id=$1
  local src_bin=$2
  local dst_name=$3
  local emu_dir="${TARGET_DIR}/plumos/emulators/${id}"
  local dst_bin="${emu_dir}/bin/${dst_name}"

  [ -f "${src_bin}" ] || return 1
  mkdir -p "${emu_dir}/bin" "${TARGET_DIR}/plumos/lib"
  install -m 0755 "${src_bin}" "${dst_bin}" || return 1
  "${STRIP}" "${dst_bin}" >/dev/null 2>&1 || true
  copy_runtime_deps "${dst_bin}" "${TARGET_DIR}/plumos/lib"

  append_manifest "  output=plumos/emulators/${id}/bin/${dst_name}"
  append_manifest "  needed="
  "${READELF}" -d "${dst_bin}" 2>/dev/null |
    awk '/NEEDED/ {print "    " $0}' >>"${MANIFEST}" || true
}

stage_license_files() {
  local id=$1
  local work=$2
  local file
  for file in COPYING COPYING.txt LICENSE LICENSE.txt LICENSE.md COPYING.md Copying README.md; do
    if [ -f "${work}/${file}" ]; then
      cp "${work}/${file}" "${TARGET_DIR}/docs/${id}-${file}" 2>/dev/null || true
      return 0
    fi
  done
  return 0
}

find_one_binary() {
  local dir=$1
  shift
  local name
  local found
  for name in "$@"; do
    found=$(find "${dir}" -type f -name "${name}" -perm -111 2>/dev/null | head -n 1)
    if [ -n "${found}" ]; then
      printf '%s\n' "${found}"
      return 0
    fi
  done
  return 1
}

normalize_ppsspp_a30_langregion() {
  local langregion=$1
  [ -f "${langregion}" ] || return 0

  # PPSSPP's stock language picker uses native-language labels. The A30 SDL
  # build does not have a full UI font fallback path, so non-Latin labels can
  # become unreadable. Keep the selectable locale ids, but show ASCII labels.
  cat >"${langregion}" <<'PPSSPP_LANGREGION_EOF'
[LangRegionNames]
ja_JP = "Japanese"
en_US = "English"
fi_FI = "Finnish"
fr_FR = "French"
es_ES = "Spanish (Spain)"
es_LA = "Spanish (Latin America)"
de_DE = "German"
it_IT = "Italian"
nl_NL = "Dutch"
pt_PT = "Portuguese"
pt_BR = "Portuguese (Brazil)"
ru_RU = "Russian"
ko_KR = "Korean"
ku_SO = "Kurdish Sorani"
zh_TW = "Chinese (Traditional)"
zh_CN = "Chinese (Simplified)"
ar_AE = "Arabic"
az_AZ = "Azerbaijani"
ca_ES = "Catalan"
gl_ES = "Galician"
gr_EL = "Greek"
he_IL = "Hebrew"
hu_HU = "Hungarian"
id_ID = "Indonesian"
pl_PL = "Polish"
ro_RO = "Romanian"
sv_SE = "Swedish"
tr_TR = "Turkish"
uk_UA = "Ukrainian"
vi_VN = "Vietnamese"
cz_CZ = "Czech"
tg_PH = "Tagalog"
th_TH = "Thai"
dr_ID = "Duri"
fa_IR = "Persian"
ms_MY = "Malay"
da_DK = "Danish"
no_NO = "Norwegian"
bg_BG = "Bulgarian"
lt-LT = "Lithuanian"
jv_ID = "Javanese"
lo_LA = "Lao"
hr_HR = "Croatian"
be_BY = "Belarusian"

[SystemLanguage]
ja_JP = "JAPANESE"
en_US = "ENGLISH"
fr_FR = "FRENCH"
es_ES = "SPANISH"
gl_ES = "SPANISH"
es_LA = "SPANISH"
de_DE = "GERMAN"
it_IT = "ITALIAN"
nl_NL = "DUTCH"
pt_PT = "PORTUGUESE"
pt_BR = "PORTUGUESE"
ru_RU = "RUSSIAN"
ko_KR = "KOREAN"
th_TH = "THAI"
zh_TW = "CHINESE_TRADITIONAL"
zh_CN = "CHINESE_SIMPLIFIED"
PPSSPP_LANGREGION_EOF
}

build_ppsspp() {
  local src=$1
  local build_dir="${src}/build-plumos"
  local bin

  case "${PPSSPP_PATCH_MODE}" in
    a30|A30)
      patch -d "${src}" -p1 < "${PATCH_DIR}/ppsspp-1.20.4-a30-display-rotation.patch" || return 1
      append_manifest "  patch=ppsspp-1.20.4-a30-display-rotation.patch"
      ;;
    display-ui|display|ui|a30-display-ui)
      patch -d "${src}" -p1 < "${PATCH_DIR}/ppsspp-1.20.4-a30-display-ui.patch" || return 1
      append_manifest "  patch=ppsspp-1.20.4-a30-display-ui.patch"
      ;;
    vanilla|none|0|no|NO|false|FALSE)
      append_manifest "  patch=none"
      ;;
    *)
      msg "invalid PPSSPP_PATCH_MODE=${PPSSPP_PATCH_MODE}"
      return 1
      ;;
  esac
  patch -d "${src}" -p1 < "${PATCH_DIR}/ppsspp-1.20.4-a30-ui-font-fallback.patch" || return 1
  append_manifest "  patch=ppsspp-1.20.4-a30-ui-font-fallback.patch"

  run_cmake "${src}" "${build_dir}" \
    -DFORCED_CPU=armv7 \
    -DARM=ON \
    -DARMV7=ON \
    -DUSING_EGL=ON \
    -DUSING_FBDEV=ON \
    -DUSING_GLES2=ON \
    -DUSING_X11_VULKAN=OFF \
    -DUSE_WAYLAND_WSI=OFF \
    -DUSE_VULKAN_DISPLAY_KHR=OFF \
    -DUSE_FFMPEG="${PPSSPP_USE_FFMPEG}" \
    -DUSE_SYSTEM_FFMPEG=OFF \
    -DUSE_DISCORD=OFF \
    -DUSE_MINIUPNPC=OFF \
    -DUSE_SYSTEM_LIBSDL2=ON \
    -DUSE_SYSTEM_LIBPNG=ON \
    -DUSE_SYSTEM_FREETYPE=OFF \
    -DUSE_SYSTEM_LIBCHDR=OFF \
    -DUSE_SYSTEM_LIBZIP=OFF \
    -DUSE_SYSTEM_MINIUPNPC=OFF \
    -DUSE_SYSTEM_SNAPPY=OFF \
    -DUSE_SYSTEM_ZSTD=OFF \
    -DHEADLESS=OFF \
    -DUNITTEST=OFF \
    -DATLAS_TOOL=OFF \
    -DUSING_QT_UI=OFF \
    -DMOBILE_DEVICE=OFF \
    -DGOLD=OFF || return 1

  cmake --build "${build_dir}" --target PPSSPPSDL -j"${JOBS}" || return 1
  bin=$(find_one_binary "${build_dir}" PPSSPPSDL) || return 1
  stage_binary "${PPSSPP_STAGE_ID}" "${bin}" "${PPSSPP_BINARY_NAME}" || return 1

  # PPSSPP still uses the stock SDL2 video path on A30, but newer PPSSPP builds
  # require SDL2_ttf symbols that the stock SDL2_ttf does not provide. Keep the
  # stock SDL2 ordering and inject only the newer SDL2_ttf ahead of it.
  local ppsspp_lib_dir="${TARGET_DIR}/plumos/emulators/${PPSSPP_STAGE_ID}/lib"
  local ppsspp_ttf_dep
  mkdir -p "${ppsspp_lib_dir}"
  for ppsspp_ttf_dep in "${TARGET_DIR}/plumos/lib"/libSDL2_ttf-2.0.so.0*; do
    [ -e "${ppsspp_ttf_dep}" ] || continue
    install -m 0755 "${ppsspp_ttf_dep}" "${ppsspp_lib_dir}/" || return 1
    append_manifest "  lib=plumos/emulators/${PPSSPP_STAGE_ID}/lib/${ppsspp_ttf_dep##*/}"
  done

  mkdir -p "${TARGET_DIR}/plumos/emulators/${PPSSPP_STAGE_ID}"
  if [ -d "${src}/assets" ]; then
    rsync -a --delete "${src}/assets/" "${TARGET_DIR}/plumos/emulators/${PPSSPP_STAGE_ID}/assets/"
    normalize_ppsspp_a30_langregion "${TARGET_DIR}/plumos/emulators/${PPSSPP_STAGE_ID}/assets/langregion.ini" || return 1
    append_manifest "  data=plumos/emulators/${PPSSPP_STAGE_ID}/assets"
  fi
}

create_scummvm_a30_theme() {
  local src=$1
  local data_dir=$2
  local theme_work="${BUILD_ROOT}/standalone-emulators/scummvm-a30-theme-md"
  local theme_dir="${theme_work}/scummmodern-a30-md"

  rm -rf "${theme_work}"
  mkdir -p "${theme_dir}" || return 1
  unzip -q "${src}/gui/themes/scummmodern.zip" -d "${theme_dir}" || return 1

  if [ -f "${src}/gui/themes/fonts/LiberationSans-Regular.ttf" ]; then
    install -m 0644 "${src}/gui/themes/fonts/LiberationSans-Regular.ttf" "${theme_dir}/"
  fi
  if [ -f "${src}/gui/themes/fonts/LiberationSans-Bold.ttf" ]; then
    install -m 0644 "${src}/gui/themes/fonts/LiberationSans-Bold.ttf" "${theme_dir}/"
  fi

  perl -0pi -e 's/ScummVM Modern Theme/ScummVM Modern A30 MD Theme/' \
    "${theme_dir}/THEMERC" || return 1
  perl -0pi -e "s/(<font\\s+resolution = 'y>=H'\\s+id = 'text_(?:default|button|normal)'\\s+file = 'helvb12\\.bdf')>/\$1\\n\\t\\t\\t\\tpoint_size = '19'>/g" \
    "${theme_dir}/scummmodern_gfx.stx" || return 1
  perl -0pi -e "s/<def var = 'Line\\.Height' value = '16' scalable = 'yes'\\/>/<def var = 'Line.Height' value = '25' scalable = 'yes'\\/>/; s/<def var = 'Font\\.Height' value = '12' scalable = 'yes'\\/>/<def var = 'Font.Height' value = '19' scalable = 'yes'\\/>/; s/size = '108, 24'/size = '108, 35'/g; s/size = '216, 24'/size = '216, 35'/g; s/size = '-1, 18'/size = '-1, 28'/g; s/size = '-1, 19'/size = '-1, 30'/g" \
    "${theme_dir}/highres_layout.stx" || return 1

  (
    cd "${theme_dir}" &&
      zip -qr "${data_dir}/scummmodern-a30-md.zip" .
  ) || return 1
}

build_scummvm() {
  local src=$1
  local bin
  local data_dir="${TARGET_DIR}/plumos/emulators/scummvm/share/scummvm"
  local file

  patch -d "${src}" -p1 < "${PATCH_DIR}/scummvm-2026.2.0-rotation-mouse-warp.patch" || return 1

  (
    cd "${src}" &&
      env \
        CC="${CC}" \
        CXX="${CXX}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        STRIP="${STRIP}" \
        SDL_CONFIG=sdl2-config \
        PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig \
        PKG_CONFIG_ALLOW_CROSS=1 \
        CFLAGS="${COMMON_CFLAGS}" \
        CXXFLAGS="${COMMON_CXXFLAGS}" \
        LDFLAGS="${COMMON_LDFLAGS}" \
        ./configure \
          --host=arm-linux-gnueabihf \
          --backend=sdl \
          --prefix=/mnt/SDCARD/plumos/emulators/scummvm \
          --enable-release \
          --disable-debug \
          --disable-all-engines \
          --enable-engine="${SCUMMVM_ENGINES}" \
          --enable-ext-neon \
          --disable-mt32emu \
          --disable-seq-midi \
          --disable-timidity \
          --disable-lua \
          --disable-nuked-opl \
          --disable-hq-scalers \
          --disable-translation \
          --disable-taskbar \
          --disable-cloud \
          --disable-system-dialogs \
          --disable-eventrecorder \
          --disable-tts \
          --disable-bink \
          --opengl-mode=none \
          --disable-tinygl \
          --disable-alsa \
          --disable-vorbis \
          --disable-tremor \
          --disable-mad \
          --disable-fribidi \
          --disable-flac \
          --disable-mpeg2 \
          --disable-mikmod \
          --disable-openmpt \
          --disable-mpcdec \
          --disable-a52 \
          --disable-opengl-game \
          --disable-gif \
          --disable-theoradec \
          --disable-vpx \
          --disable-faad \
          --disable-fluidsynth \
          --disable-fluidlite \
          --disable-sonivox \
          --disable-freetype2 \
          --disable-readline \
          --disable-gtk \
          --disable-sndio &&
      make -j"${JOBS}"
  ) || return 1

  bin=$(find_one_binary "${src}" scummvm) || return 1
  stage_binary scummvm "${bin}" scummvm || return 1

  mkdir -p "${data_dir}"
  for file in \
    gui/themes/gui-icons.dat \
    gui/themes/scummclassic.zip \
    gui/themes/scummmodern.zip \
    gui/themes/scummremastered.zip \
    gui/themes/residualvm.zip \
    gui/themes/shaders.dat \
    gui/themes/translations.dat \
    dists/engine-data/encoding.dat \
    dists/engine-data/drascula.dat \
    dists/engine-data/helpdialog.zip \
    dists/engine-data/kyra.dat \
    dists/engine-data/lure.dat \
    dists/engine-data/queen.tbl \
    dists/engine-data/sky.cpt \
    dists/engine-data/teenagent.dat \
    backends/vkeybd/packs/vkeybd_default.zip \
    backends/vkeybd/packs/vkeybd_small.zip; do
    if [ -f "${src}/${file}" ]; then
      install -m 0644 "${src}/${file}" "${data_dir}/$(basename "${file}")"
    fi
  done

  create_scummvm_a30_theme "${src}" "${data_dir}" || return 1

  append_manifest "  data=plumos/emulators/scummvm/share/scummvm"
}

build_inih_for_easyrpg() {
  local src=$1
  local inih_dir="${src}/lib/inih"

  rm -rf "${inih_dir}"
  mkdir -p "$(dirname "${inih_dir}")"
  git clone --depth 1 https://github.com/benhoyt/inih.git "${inih_dir}" || return 1
  (
    cd "${inih_dir}" &&
      env CC="${CC}" CFLAGS="${COMMON_CFLAGS} -fPIC" "${CC}" -c ini.c -o ini.o &&
      "${AR}" rcs libinih.a ini.o &&
      "${RANLIB}" libinih.a
  ) || return 1
}

build_easyrpg() {
  local src=$1
  local build_dir="${src}/build-plumos"
  local bin

  patch -d "${src}" -p1 < "${PATCH_DIR}/easyrpg-0.8.1.1-a30-display-rotation.patch" || return 1
  append_manifest "  patch=easyrpg-0.8.1.1-a30-display-rotation.patch"

  build_inih_for_easyrpg "${src}" || return 1
  run_cmake "${src}" "${build_dir}" \
    -DPLAYER_TARGET_PLATFORM=SDL2 \
    -DPLAYER_AUDIO_BACKEND=SDL2 \
    -DPLAYER_BUILD_EXECUTABLE=ON \
    -DPLAYER_BUILD_LIBLCF=ON \
    -DPLAYER_BUILD_LIBLCF_BRANCH=0.8.1 \
    -DINIH_INCLUDE_DIR="${src}/lib/inih" \
    -DINIH_LIBRARY="${src}/lib/inih/libinih.a" \
    -DLIBLCF_WITH_ICU=ON \
    -DLIBLCF_WITH_XML=ON \
    -DLIBLCF_ENABLE_TOOLS=OFF \
    -DLIBLCF_ENABLE_TESTS=OFF \
    -DLIBLCF_ENABLE_BENCHMARKS=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DPLAYER_WITH_FREETYPE=ON \
    -DPLAYER_WITH_HARFBUZZ=ON \
    -DPLAYER_WITH_LHASA=ON \
    -DPLAYER_WITH_MPG123=ON \
    -DPLAYER_WITH_LIBSNDFILE=ON \
    -DPLAYER_WITH_OGGVORBIS=ON \
    -DPLAYER_WITH_OPUS=ON \
    -DPLAYER_WITH_WILDMIDI=OFF \
    -DPLAYER_WITH_FLUIDSYNTH=OFF \
    -DPLAYER_WITH_FLUIDLITE=OFF \
    -DPLAYER_WITH_XMP=ON \
    -DPLAYER_WITH_NATIVE_MIDI=OFF \
    -DPLAYER_WITH_SPEEXDSP=ON \
    -DPLAYER_WITH_SAMPLERATE=ON || return 1

  cmake --build "${build_dir}" --target easyrpg-player -j"${JOBS}" || return 1
  bin=$(find_one_binary "${build_dir}" easyrpg-player) || return 1
  stage_binary easyrpg "${bin}" easyrpg-player || return 1
}

build_dosbox() {
  local src=$1
  local build_dir="${src}/build-plumos"
  local cross_file="${build_dir}/plumos-armhf.ini"
  local c_args cxx_args c_link_args cxx_link_args
  local bin
  local resource_dir="${TARGET_DIR}/plumos/emulators/dosbox-staging/resources"

  rm -rf "${build_dir}"
  mkdir -p "${build_dir}"
  patch -d "${src}" -p1 < "${PATCH_DIR}/dosbox-staging-0.82.2-cross-speexdsp.patch" || return 1
  patch -d "${src}" -p1 < "${PATCH_DIR}/dosbox-staging-0.82.2-a30-display-rotation.patch" || return 1
  append_manifest "  patch=dosbox-staging-0.82.2-a30-display-rotation.patch"
  c_args=$(meson_word_array "${COMMON_CFLAGS}")
  cxx_args=$(meson_word_array "${COMMON_CXXFLAGS}")
  c_link_args=$(meson_word_array "${COMMON_LDFLAGS}")
  cxx_link_args=$(meson_word_array "${COMMON_LDFLAGS}")
  cat >"${cross_file}" <<EOF
[binaries]
c = '$(tool_path "${CC}")'
cpp = '$(tool_path "${CXX}")'
ar = '$(tool_path "${AR}")'
strip = '$(tool_path "${STRIP}")'
pkgconfig = 'pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7'
endian = 'little'

[properties]
needs_exe_wrapper = true
pkg_config_libdir = ['/usr/lib/arm-linux-gnueabihf/pkgconfig', '/usr/share/pkgconfig']
c_args = [${c_args}]
cpp_args = [${cxx_args}]
c_link_args = [${c_link_args}]
cpp_link_args = [${cxx_link_args}]
EOF

  PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig \
  PKG_CONFIG_ALLOW_CROSS=1 \
  meson setup "${build_dir}" "${src}" \
    --cross-file "${cross_file}" \
    --prefix=/mnt/SDCARD/plumos/emulators/dosbox-staging \
    --buildtype=release \
    -Duse_sdl2_net=false \
    -Duse_opengl=false \
    -Duse_fluidsynth=false \
    -Duse_mt32emu=false \
    -Duse_slirp=false \
    -Duse_alsa=true \
    -Duse_xinput2=false \
    -Ddynamic_core=dynrec \
    -Dper_page_w_or_x=disabled \
    -Dpagesize=4096 \
    -Duse_zlib_ng=false \
    -Dunit_tests=disabled || return 1

  meson compile -C "${build_dir}" -j"${JOBS}" || return 1
  bin=$(find_one_binary "${build_dir}" dosbox) || return 1
  stage_binary dosbox-staging "${bin}" dosbox || return 1
  if [ -d "${build_dir}/resources" ]; then
    mkdir -p "${resource_dir}"
    rsync -a --delete "${build_dir}/resources/" "${resource_dir}/"
    append_manifest "  data=plumos/emulators/dosbox-staging/resources"
  fi
}

build_pcsx_rearmed() {
  local src=$1

  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-maemo-cdfile-lifetime.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-maemo-cdfile-lifetime.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-fbcon-safe-mode.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-fbcon-safe-mode.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-fbcon-mode-guard.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-fbcon-mode-guard.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-native-fb32-rotation.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-native-fb32-rotation.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-native-fb32-flip-mode-fix.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-native-fb32-flip-mode-fix.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-sw-surface-keyboard.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-sw-surface-keyboard.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-menu-output-zoom.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-menu-output-zoom.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-menu-return-shadow-clear.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-menu-return-shadow-clear.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-menu-landscape-virtual.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-menu-landscape-virtual.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-bios-dir-env.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-bios-dir-env.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/pcsx_rearmed-r26l-a30-menu-scale-env.patch" || return 1
  append_manifest "  patch=pcsx_rearmed-r26l-a30-menu-scale-env.patch"
  grep -q 'set_bpp = 32' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX native fb32 mode patch did not apply"
    return 1
  }
  grep -q 'plumos_a30_blit_rgb565_rotated_xrgb8888(shadow_fb' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX native fb32 flip patch did not apply"
    return 1
  }
  grep -q 'screen_flags = SDL_SWSURFACE' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX SDL software surface patch did not apply"
    return 1
  }
  grep -q 'SDLK_WORLD_0,  IN_BINDTYPE_PLAYER12, DKEY_CROSS' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX A30 gamepad bind patch did not apply"
    return 1
  }
  grep -q 'PLUMOS_A30_PCSX_MENU_SCALE' "${src}/frontend/menu.c" || {
    msg "error: PCSX menu scale patch did not apply"
    return 1
  }
  grep -q 'PLUMOS_A30_PCSX_MENU_ZOOM' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX menu output zoom patch did not apply"
    return 1
  }
  grep -q 'PLUMOS_A30_PCSX_MENU_EDGE_MASK' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX menu edge mask patch did not apply"
    return 1
  }
  grep -q 'SDLK_ESCAPE, PBTN_MENU' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX menu return keymap patch did not apply"
    return 1
  }
  grep -q 'shadow_fb_size' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX shadow clear patch did not apply"
    return 1
  }
  grep -q 'PLUMOS_A30_PCSX_MENU_LANDSCAPE' "${src}/frontend/plat_sdl.c" || {
    msg "error: PCSX menu landscape virtual patch did not apply"
    return 1
  }

  (
    cd "${src}" &&
      make clean >/dev/null 2>&1 || true &&
      env \
        CROSS_COMPILE="${CROSS_PREFIX}" \
        CC="${CC}" \
        CXX="${CXX}" \
        AS="${CROSS_PREFIX}as" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        STRIP="${STRIP}" \
        SDL_CONFIG=sdl-config \
        CFLAGS="${COMMON_CFLAGS}" \
        CXXFLAGS="${COMMON_CXXFLAGS}" \
        ASFLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard" \
        LDFLAGS="${COMMON_LDFLAGS}" \
        ./configure \
          --platform=generic \
          --gpu=neon \
          --sound-drivers="alsa sdl" \
          --enable-neon \
          --enable-threads \
          --disable-dynamic \
          --dynarec=ari64 &&
      ! grep -Eq '^SOUND_DRIVERS[[:space:]]*=.*(^|[[:space:]])oss([[:space:]]|$)' config.mak &&
      grep -Eq '^SOUND_DRIVERS[[:space:]]*=.*(^|[[:space:]])alsa([[:space:]]|$)' config.mak &&
      env \
        CROSS_COMPILE="${CROSS_PREFIX}" \
        CC="${CC}" \
        CXX="${CXX}" \
        AR="${AR}" \
        RANLIB="${RANLIB}" \
        STRIP="${STRIP}" \
        make -j"${JOBS}"
  ) || return 1

  stage_binary pcsx_rearmed "${src}/pcsx" pcsx || return 1
}

build_red_viper() {
  local src=$1
  local build_dir="${src}/build-plumos-a30"
  local wrapper_dir="${RED_VIPER_A30_FRONTEND_DIR}"
  local out="${build_dir}/red-viper-a30"
  local red_cflags red_cxxflags red_asflags
  local red_audio_rate
  local objects=()

  [ -d "${wrapper_dir}" ] || {
    msg "missing Red Viper A30 frontend sources: ${wrapper_dir}"
    return 1
  }

  rm -rf "${build_dir}"
  mkdir -p "${build_dir}"

  red_audio_rate=${RED_VIPER_A30_AUDIO_RATE:-48000}
  case "${red_audio_rate}" in
    48000|50000)
      ;;
    *)
      msg "unsupported Red Viper A30 audio rate: ${red_audio_rate}"
      return 1
      ;;
  esac
  if [ "${red_audio_rate}" != 50000 ]; then
    sed -i "s/#define SAMPLE_RATE 50000/#define SAMPLE_RATE ${red_audio_rate}/g" \
      "${src}/include/vb_sound.h" "${src}/source/common/vb_sound.c"
  fi

  red_cflags="${COMMON_CFLAGS} -O3 -std=gnu11 -DDEBUGLEVEL=0 -I${wrapper_dir}/include -I${src}/include -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-truncation -Wno-stringop-truncation -Wno-implicit-fallthrough -pthread"
  red_cxxflags="${COMMON_CXXFLAGS} -O3 -std=gnu++17 -DDEBUGLEVEL=0 -I${wrapper_dir}/include -I${src}/include -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-truncation -Wno-stringop-truncation -Wno-implicit-fallthrough -fno-exceptions -fno-rtti -pthread"
  red_asflags="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"

  compile_c() {
    local rel=$1
    local obj="${build_dir}/${rel//\//_}.o"
    "${CC}" ${red_cflags} -c "${src}/${rel}" -o "${obj}" || return 1
    objects+=("${obj}")
  }

  compile_cxx() {
    local rel=$1
    local obj="${build_dir}/${rel//\//_}.o"
    "${CXX}" ${red_cxxflags} -c "${src}/${rel}" -o "${obj}" || return 1
    objects+=("${obj}")
  }

  compile_asm() {
    local rel=$1
    local obj="${build_dir}/${rel//\//_}.o"
    "${CC}" ${red_asflags} -c "${src}/${rel}" -o "${obj}" || return 1
    objects+=("${obj}")
  }

  compile_wrapper_c() {
    local rel=$1
    local obj="${build_dir}/wrapper_${rel//\//_}.o"
    "${CC}" ${red_cflags} -c "${wrapper_dir}/${rel}" -o "${obj}" || return 1
    objects+=("${obj}")
  }

  compile_wrapper_c main.c || return 1
  compile_wrapper_c replay_stubs.c || return 1
  compile_wrapper_c sound_stubs.c || return 1
  compile_wrapper_c video_stubs.c || return 1

  compile_c source/common/interpreter.c || return 1
  compile_c source/common/patches.c || return 1
  compile_c source/common/rom_db.c || return 1
  compile_c source/common/v810_cpu.c || return 1
  compile_c source/common/v810_ins.c || return 1
  compile_c source/common/v810_mem.c || return 1
  compile_c source/common/vb_set.c || return 1
  compile_c source/common/vb_sound.c || return 1
  compile_c source/common/video_common.c || return 1
  compile_c source/arm/drc_alloc.c || return 1
  compile_c source/arm/drc_core.c || return 1
  compile_c source/linux-test/arm_utils.c || return 1
  compile_cxx source/common/video_soft.cpp || return 1
  compile_asm source/arm/drc_exec.s || return 1
  compile_asm source/arm/drc_static.s || return 1

  "${CXX}" ${COMMON_LDFLAGS} -pthread -o "${out}" "${objects[@]}" -lasound -lm || return 1
  stage_binary red_viper "${out}" red-viper-a30 || return 1

  append_manifest "  frontend=red-viper-a30 fbdev/input wrapper"
  append_manifest "  audio=alsa-default"
  append_manifest "  audio_rate=${red_audio_rate}"
  append_manifest "  audio_queue=threaded-gap-fill"

  patch -d "${src}" -p1 < "${PATCH_DIR}/red-viper-a30-sdlgl-present.patch" || return 1
  append_manifest "  patch=red-viper-a30-sdlgl-present.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/red-viper-a30-sdlgl-env-options.patch" || return 1
  append_manifest "  patch=red-viper-a30-sdlgl-env-options.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/red-viper-a30-sdlgl-audio-queue-env.patch" || return 1
  append_manifest "  patch=red-viper-a30-sdlgl-audio-queue-env.patch"

  build_dir="${src}/build-plumos-a30-sdlgl"
  out="${build_dir}/red-viper-sdlgl-a30"
  rm -rf "${build_dir}"
  mkdir -p "${build_dir}"
  objects=()

  compile_wrapper_c replay_stubs.c || return 1
  compile_wrapper_c sdlgl_stats.c || return 1

  compile_c source/common/interpreter.c || return 1
  compile_c source/common/patches.c || return 1
  compile_c source/common/rom_db.c || return 1
  compile_c source/common/v810_cpu.c || return 1
  compile_c source/common/v810_ins.c || return 1
  compile_c source/common/v810_mem.c || return 1
  compile_c source/common/vb_set.c || return 1
  compile_c source/common/vb_sound.c || return 1
  compile_c source/common/video.c || return 1
  compile_c source/common/video_common.c || return 1
  compile_c source/common/video_hard.c || return 1
  compile_c source/arm/drc_alloc.c || return 1
  compile_c source/arm/drc_core.c || return 1
  compile_c source/linux-test/arm_utils.c || return 1
  compile_c source/linux-test/main.c || return 1
  compile_c source/linux-test/opengl.c || return 1
  compile_c source/linux-test/sound.c || return 1
  compile_cxx source/common/video_soft.cpp || return 1
  compile_asm source/arm/drc_exec.s || return 1
  compile_asm source/arm/drc_static.s || return 1

  "${CXX}" ${COMMON_LDFLAGS} -pthread \
    -Wl,--wrap=SDL_GL_SwapWindow \
    -o "${out}" "${objects[@]}" -lSDL2 -lGLESv2 -lm || return 1
  stage_binary red_viper "${out}" red-viper-sdlgl-a30 || return 1

  append_manifest "  frontend=red-viper-sdlgl-a30 stock-sdl2-mali-gles2"
  append_manifest "  display=gles-final-quad-rotation"
}

build_openbor() {
  local src=$1
  local engine_dir="${src}/engine"
  local bin="${engine_dir}/OpenBOR"
  local openbor_archflags

  patch -d "${src}" -p1 < "${PATCH_DIR}/openbor-v6391-a30-sdl2-rotation.patch" || return 1
  append_manifest "  patch=openbor-v6391-a30-sdl2-rotation.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/openbor-v6391-a30mali-renderer.patch" || return 1
  append_manifest "  patch=openbor-v6391-a30mali-renderer.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/openbor-v6391-a30-direct-present.patch" || return 1
  append_manifest "  patch=openbor-v6391-a30-direct-present.patch"
  patch -d "${src}" -p1 < "${PATCH_DIR}/openbor-v6391-a30-p1-controls.patch" || return 1
  append_manifest "  patch=openbor-v6391-a30-p1-controls.patch"

  openbor_archflags="-marm -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fcommon"
  sed -i 's/-Wall -Werror/-Wall/g' "${engine_dir}/Makefile" || return 1
  append_manifest "  patch=makefile-drop-werror"

  (
    cd "${engine_dir}" || exit 1
    make clean BUILD_LINUX=1 >/dev/null 2>&1 || true
    PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig \
    PKG_CONFIG_ALLOW_CROSS=1 \
    CFLAGS="${COMMON_CFLAGS}" \
    LDFLAGS="${COMMON_LDFLAGS}" \
    make -j"${JOBS}" \
      BUILD_LINUX=1 \
      BUILD_MMX= \
      BUILD_OPENGL= \
      BUILD_LOADGL= \
      BUILD_WEBM= \
      NO_STRIP=1 \
      VERSION_NAME=OpenBOR \
      LNXDEV=/usr/bin \
      PREFIX="${CROSS_PREFIX}" \
      GCC_TARGET=arm-linux-gnueabihf \
      TARGET_ARCH=arm \
      ARCHFLAGS="${openbor_archflags}" \
      LIBRARIES=/usr/lib/arm-linux-gnueabihf \
      CC="${CC}"
  ) || return 1

  stage_binary openbor "${bin}" OpenBOR || return 1
  append_manifest "  video=plumos-sdl2-input-audio a30-direct-mali-presenter"
  append_manifest "  webm=disabled"
}

prepare_dist() {
  rm -rf "${TARGET_DIR}"
  mkdir -p "${TARGET_DIR}/docs/build-logs" "${TARGET_DIR}/plumos/bin"
  MANIFEST="${TARGET_DIR}/docs/manifest.txt"
  LOG_DIR="${TARGET_DIR}/docs/build-logs"

  {
    echo "plumOS standalone emulator build"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "cc=$("${CC}" --version | head -n 1)"
    echo "cflags=${COMMON_CFLAGS}"
    echo "filter=${PLUMOS_STANDALONE_FILTER}"
    echo "include_experimental=${PLUMOS_STANDALONE_INCLUDE_EXPERIMENTAL}"
    echo "target_dir=${TARGET_DIR}"
    echo "ppsspp_patch_mode=${PPSSPP_PATCH_MODE}"
    echo "ppsspp_stage_id=${PPSSPP_STAGE_ID}"
    echo "ppsspp_binary_name=${PPSSPP_BINARY_NAME}"
    echo "scummvm_engines=${SCUMMVM_ENGINES}"
    echo "red_viper_ref=${RED_VIPER_REF}"
    echo "red_viper_a30_audio_rate=${RED_VIPER_A30_AUDIO_RATE:-48000}"
    echo "openbor_ref=${OPENBOR_REF}"
    echo
  } >"${MANIFEST}"
}

write_standalone_launcher() {
  local launcher="${TARGET_DIR}/plumos/bin/plumos-standalone-launch"

  cat >"${launcher}" <<'EOF'
#!/bin/sh
set -u

usage() {
  cat <<USAGE
Usage: plumos-standalone-launch EMULATOR [ROM_OR_ARGS...]

EMULATOR:
  ppsspp
  ppsspp-display-ui
  ppsspp-vanilla
  scummvm
  easyrpg
  openbor
  dosbox-staging | dosbox
  pcsx_rearmed | pcsx
  red_viper | red-viper

Environment:
  PLUMOS_ROOT                         Default: /mnt/SDCARD/plumos
  PLUMOS_ROOT/config/standalone/standalone.env
                                      Optional global standalone launcher env overrides.
  PLUMOS_ROOT/config/standalone/EMULATOR.env
                                      Optional per-emulator launcher env overrides.
  PLUMOS_STANDALONE_USE_STOCK_SDL=0   Prefer plumOS SDL2 instead of stock SDL.
  PLUMOS_STANDALONE_JOYSTICKD=0       Disable per-launch plumOS gamepad daemon.
  PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE buttons or axes for L2/R2. Default: buttons.
  PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT standard or user.
  PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE
                                      passthrough, pcsx-menu-l2, dosbox, or digger.
  PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE axisYL, axisXL, axisYR, or axisXR.
  PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE axisYL, axisXL, axisYR, or axisXR.
  PLUMOS_STANDALONE_JOYSTICKD_INVERT_X=1 Invert virtual gamepad X axis.
  PLUMOS_STANDALONE_JOYSTICKD_INVERT_Y=1 Invert virtual gamepad Y axis.
  PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON mode, thumbl, thumbr, none.
  PLUMOS_STANDALONE_CPU_POLICY        CPU policy: keep, performance, fixed.
  PLUMOS_STANDALONE_CPU_FREQ          Fixed CPU frequency in kHz.
  PLUMOS_STANDALONE_CPU_CORES         CPU core policy: keep, 2, 4.
  PLUMOS_A30_PSP_CPU_POLICY           PPSSPP CPU policy. Default: fixed.
  PLUMOS_A30_PSP_CPU_FREQ             PPSSPP fixed CPU frequency. Default: 1344000.
  PLUMOS_A30_PSP_CPU_CORES            PPSSPP CPU core policy. Default: 4.
  PLUMOS_A30_DISPLAY_ROTATION         PPSSPP display rotation: ccw, cw, none. Default: ccw.
  PLUMOS_A30_DISPLAY_SWAP=1           Swap PPSSPP display axes after rotation. Default: 0.
  PLUMOS_A30_DISPLAY_LOGICAL=854x480  Override PPSSPP layout dp size without swapping framebuffer pixels.
  PLUMOS_A30_DISPLAY_OUTPUT=640x480   Override PPSSPP game output size for aspect experiments.
  PLUMOS_A30_DISPLAY_RECT_TRACE=1     Log PPSSPP output rect calculations for aspect debugging.
  PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE  Make PPSSPP choose landscape UI layout. Default: 1.
  PLUMOS_A30_UI_ROTATION              Rotate PPSSPP UI only: none, ccw, cw, 180. Default: none.
  PLUMOS_A30_PSP_APPEND_PROFILE=1     Enable legacy PPSSPP append profile. Default: disabled.
  PLUMOS_A30_PSP_UI_SCALE             Legacy append-profile UI scale. Used only when append profile is enabled.
  PLUMOS_A30_PSP_PROFILE              Legacy append profile: standard, none.
  PLUMOS_A30_PSP_LAYOUT               Legacy append layout: a30-landscape, portrait-vertical, none.
  PLUMOS_A30_PSP_MENU_BUTTON          PPSSPP pause menu button: function, l2, r2, none. Default: function.
  PLUMOS_A30_PSP_CONTROLS=standard    Repair/create PPSSPP controls.ini. Default: none.
  PLUMOS_A30_PSP_FORCE_CONTROLS=1     Replace PPSSPP controls.ini when controls repair is enabled.
  PLUMOS_A30_PSP_ESCAPE_EXIT=1        Make PPSSPP exit when a Pause-mapped key is pressed.
  PLUMOS_A30_PSP_RESET_INSTANCE_COUNTER=0 Do not clear stale PPSSPP audio-silencing state.
  PLUMOS_A30_PSX_BIOS_DIR             PCSX-ReARMed BIOS directory. Default: /mnt/SDCARD/Bios.
  PLUMOS_A30_SCUMMVM_DEFAULT_CONFIG=0 Do not inject ScummVM A30 defaults when --config is absent.
  PLUMOS_A30_SCUMMVM_ROTATION         ScummVM rotation_mode default. Default: 270.
  PLUMOS_A30_SCUMMVM_GUI_THEME        ScummVM GUI theme default. Default: scummmodern-a30-md.
  PLUMOS_A30_SCUMMVM_FULLSCREEN       ScummVM fullscreen default. Default: true.
  PLUMOS_A30_SCUMMVM_STRETCH_MODE     ScummVM stretch_mode default. Default: fit.
  PLUMOS_A30_SCUMMVM_TARGET           Override ScummVM target id for directory ROMs.
  PLUMOS_A30_EASYRPG_ROTATION         EasyRPG final display rotation: ccw, cw, 180, none. Default: ccw.
  PLUMOS_A30_OPENBOR_CPU_POLICY       OpenBOR CPU policy. Default: fixed.
  PLUMOS_A30_OPENBOR_CPU_FREQ         OpenBOR fixed CPU frequency. Default: 1344000.
  PLUMOS_A30_OPENBOR_CPU_CORES        OpenBOR CPU core policy. Default: 4.
  PLUMOS_A30_OPENBOR_ROTATION         OpenBOR final display rotation: ccw, cw, 180, none. Default: ccw.
  PLUMOS_A30_OPENBOR_SCALE            OpenBOR scaling: fit, stretch, integer, native. Default: fit.
  PLUMOS_A30_OPENBOR_FULLSCREEN       OpenBOR fullscreen desktop mode: 0 or 1. Default: 1.
  PLUMOS_A30_OPENBOR_DIRECT_PRESENT   Route OpenBOR frames directly to fbdev+Mali. Default: 1 when not using stock SDL.
  PLUMOS_A30_OPENBOR_DIRECT_ROTATION  Direct presenter rotation: ccw, cw, 180, none. Default: ccw.
  PLUMOS_A30_OPENBOR_DIRECT_LOGICAL_SIZE
                                      Direct presenter logical canvas. Default: 640x480.
  PLUMOS_A30_OPENBOR_FORCE_FB_FRONT_PAGE
                                      Force fb0 pan to 0,0 during OpenBOR startup. Default: 1.
  PLUMOS_A30_OPENBOR_FIX_CONTROLS     Patch P1 keyboard defaults to A30 joystick controls. Default: 1.
  PLUMOS_A30_OPENBOR_FORCE_CONTROLS   Always patch P1 A30 joystick controls. Default: 0.
  PLUMOS_A30_DOSBOX_ROTATION          DOSBox final display rotation: ccw, cw, 180, none. Default: ccw.
  PLUMOS_A30_RED_VIPER_CPU_POLICY     Red Viper CPU policy. Default: fixed.
  PLUMOS_A30_RED_VIPER_CPU_FREQ       Red Viper fixed CPU frequency. Default: 648000.
  PLUMOS_A30_RED_VIPER_CPU_CORES      Red Viper CPU core policy. Default: 2.
  PLUMOS_A30_RED_VIPER_RENDERER       Red Viper renderer: sdlgl, fbdev. Default: sdlgl.
  PLUMOS_A30_RED_VIPER_FB             Red Viper framebuffer path. Default: /dev/fb0.
  PLUMOS_A30_RED_VIPER_INPUT          Red Viper input event path. Default: /dev/input/event3.
  PLUMOS_A30_RED_VIPER_ROTATION       Red Viper final display rotation: ccw, cw, none. Default: ccw.
  PLUMOS_A30_RED_VIPER_SCALE          Red Viper scaling: fit, stretch, integer. Default: fit.
  PLUMOS_A30_RED_VIPER_SDLGL_ROTATION Red Viper SDL/GLES final rotation: ccw, cw, 180, none. Default: ccw.
  PLUMOS_A30_RED_VIPER_SDLGL_SCALE    Red Viper SDL/GLES scaling: fit, stretch, integer. Default: fit.
  PLUMOS_A30_RED_VIPER_SDLGL_PHYSICAL Red Viper SDL/GLES framebuffer size. Default: 480x640.
  PLUMOS_A30_RED_VIPER_EYE            Red Viper eye mode: left, right, both. Default: both.
  PLUMOS_A30_RED_VIPER_COLOR          Red Viper tint: red, white, green, amber, blue, or R,G,B. Default: red.
  PLUMOS_A30_RED_VIPER_WAIT_VSYNC     Red Viper framebuffer pan vsync wait: 0 or 1. Default: 1.
  PLUMOS_A30_RED_VIPER_FRAME_SKIP     Red Viper display frame skip: 0..3. Default: 0.
  PLUMOS_A30_RED_VIPER_FAST_FORWARD   Red Viper fast-forward at launch: 0 or 1. Default: 0.
  PLUMOS_A30_RED_VIPER_FF_TOGGLE      Red Viper fast-forward mode: 0 hold, 1 toggle. Default: 0.
  PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK  Red Viper VIP overclock timing shortcut: 0 or 1. Default: 0.
  PLUMOS_A30_RED_VIPER_PERF_INFO      Red Viper A30 FPS/speed overlay: 0 or 1. Default: 0.
  PLUMOS_A30_RED_VIPER_AUDIO          Red Viper audio mode: alsa, off. Default: alsa.
  PLUMOS_A30_RED_VIPER_ALSA_DEVICE    Red Viper ALSA PCM device. Default: default.
  PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US Red Viper ALSA latency in microseconds. Default: 160000.
  PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS Red Viper ALSA startup prebuffer chunks. Default: 6.
  PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS Red Viper ALSA producer queue chunks. Default: 8.
  PLUMOS_A30_RED_VIPER_AUDIO_GAP      Red Viper audio gap fill: fade, silence, repeat. Default: fade.
  SDL_VIDEODRIVER / SDL_AUDIODRIVER   Override per-emulator defaults.
USAGE
}

if [ "$#" -lt 1 ] || [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

id=$1
shift

PLUMOS_ROOT=${PLUMOS_ROOT:-/mnt/SDCARD/plumos}
CONFIG_DIR=${PLUMOS_ROOT}/config/standalone

load_env_file() {
  env_file=$1
  [ -r "${env_file}" ] || return 0
  set -a
  . "${env_file}"
  set +a
}

load_standalone_env() {
  load_env_file "${CONFIG_DIR}/standalone.env"
  load_env_file "${CONFIG_DIR}/${id}.env"
}

load_standalone_env

EMU_ROOT=${PLUMOS_ROOT}/emulators
PLUMOS_LIB=${PLUMOS_ROOT}/lib
STOCK_LIB=${PLUMOS_STOCK_LIB:-/mnt/SDCARD/miyoo/lib:/usr/miyoo/lib}
case "${id}" in
  ppsspp|ppsspp-display-ui|ppsspp-vanilla)
    ppsspp_local_lib="${EMU_ROOT}/${id}/lib"
    if [ -d "${ppsspp_local_lib}" ]; then
      case ":${STOCK_LIB}:" in
        *":${ppsspp_local_lib}:"*) ;;
        *) STOCK_LIB="${ppsspp_local_lib}:${STOCK_LIB}" ;;
      esac
    fi
    ;;
esac
LOADER=${PLUMOS_LIB}/ld-linux-armhf.so.3

if [ ! -x "${LOADER}" ]; then
  echo "error: missing dynamic loader: ${LOADER}" >&2
  exit 127
fi

case "${PLUMOS_STANDALONE_USE_STOCK_SDL:-1}" in
  0|no|NO|false|FALSE)
    LIB_PATH=${PLUMOS_LIB}:/usr/lib:/lib
    ;;
  *)
    LIB_PATH=${STOCK_LIB}:${PLUMOS_LIB}:/usr/lib:/lib
    ;;
esac

state_id=${id}
case "${id}" in
  dosbox) state_id=dosbox-staging ;;
  pcsx) state_id=pcsx_rearmed ;;
  red-viper) state_id=red_viper ;;
esac

STATE_DIR=${PLUMOS_ROOT}/state/standalone/${state_id}
LOG_DIR=${PLUMOS_ROOT}/logs/standalone
VOLUME_CONTROL=${PLUMOS_VOLUME_CONTROL:-${PLUMOS_ROOT}/bin/plumos-volume-control}
CPU_STATE=/tmp/plumos-standalone-launch-cpustate-$$
mkdir -p "${STATE_DIR}" "${STATE_DIR}/config" "${STATE_DIR}/data" "${LOG_DIR}"

export HOME=${STATE_DIR}
export XDG_CONFIG_HOME=${STATE_DIR}/config
export XDG_DATA_HOME=${STATE_DIR}/data

joystickd_pid=
cpu_policy_applied=0
ppsspp_instance_counter_cleanup=0
standalone_tmp_paths=

cpu_freq_dir() {
  printf '%s\n' /sys/devices/system/cpu/cpu0/cpufreq
}

save_cpu_policy() {
  p=$(cpu_freq_dir)
  {
    printf "governor=%s\n" "$(cat "$p/scaling_governor" 2>/dev/null || true)"
    printf "min_freq=%s\n" "$(cat "$p/scaling_min_freq" 2>/dev/null || true)"
    printf "max_freq=%s\n" "$(cat "$p/scaling_max_freq" 2>/dev/null || true)"
    for cpu in 1 2 3; do
      if [ -r "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
        printf "cpu%s_online=%s\n" "$cpu" "$(cat "/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true)"
      fi
    done
  } >"${CPU_STATE}"
}

restore_cpu_policy() {
  [ "${cpu_policy_applied}" = 1 ] || return 0
  [ -f "${CPU_STATE}" ] || return 0
  . "${CPU_STATE}" 2>/dev/null || true

  for cpu in 1 2 3; do
    eval value="\${cpu${cpu}_online:-}"
    if [ -n "${value}" ] && [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ]; then
      echo "${value}" >"/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
    fi
  done

  p=$(cpu_freq_dir)
  if [ -n "${governor:-}" ] && [ -w "$p/scaling_governor" ]; then
    echo "${governor}" >"$p/scaling_governor" 2>/dev/null || true
  fi
  if [ -n "${min_freq:-}" ] && [ -w "$p/scaling_min_freq" ]; then
    echo "${min_freq}" >"$p/scaling_min_freq" 2>/dev/null || true
  fi
  if [ -n "${max_freq:-}" ] && [ -w "$p/scaling_max_freq" ]; then
    echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
  fi
  rm -f "${CPU_STATE}"
  cpu_policy_applied=0
}

apply_cpu_policy() {
  policy=$1
  freq=$2
  cores=$3

  case "${policy}" in
    performance|fixed) ;;
    *) echo "error: invalid CPU policy: ${policy}" >&2; exit 2 ;;
  esac
  case "${freq}" in
    ''|*[!0-9]*) echo "error: invalid CPU frequency: ${freq}" >&2; exit 2 ;;
  esac
  case "${cores}" in
    2|4) ;;
    *) echo "error: invalid CPU cores: ${cores}" >&2; exit 2 ;;
  esac

  save_cpu_policy
  cpu_policy_applied=1

  case "${cores}" in
    2)
      [ -w /sys/devices/system/cpu/cpu1/online ] &&
        echo 1 >/sys/devices/system/cpu/cpu1/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu2/online ] &&
        echo 0 >/sys/devices/system/cpu/cpu2/online 2>/dev/null || true
      [ -w /sys/devices/system/cpu/cpu3/online ] &&
        echo 0 >/sys/devices/system/cpu/cpu3/online 2>/dev/null || true
      ;;
    4)
      for cpu in 1 2 3; do
        [ -w "/sys/devices/system/cpu/cpu${cpu}/online" ] &&
          echo 1 >"/sys/devices/system/cpu/cpu${cpu}/online" 2>/dev/null || true
      done
      ;;
  esac

  p=$(cpu_freq_dir)
  case "${policy}" in
    performance)
      if [ -w "$p/scaling_max_freq" ]; then
        max_freq=$(cat "$p/cpuinfo_max_freq" 2>/dev/null || echo "${freq}")
        echo "${max_freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      fi
      echo performance >"$p/scaling_governor" 2>/dev/null || true
      ;;
    fixed)
      echo "${freq}" >"$p/scaling_max_freq" 2>/dev/null || true
      echo "${freq}" >"$p/scaling_min_freq" 2>/dev/null || true
      echo userspace >"$p/scaling_governor" 2>/dev/null || true
      [ -w "$p/scaling_setspeed" ] &&
        echo "${freq}" >"$p/scaling_setspeed" 2>/dev/null || true
      ;;
  esac
}

apply_system_volume() {
  if [ -x "${VOLUME_CONTROL}" ]; then
    "${VOLUME_CONTROL}" apply >>"${LOG_DIR}/volume-control-last.log" 2>&1 || true
  fi
}

find_resident_joystickd() {
  expected_opts=${1:-}
  for f in /proc/[0-9]*/cmdline; do
    [ -r "$f" ] || continue
    cmd=$(tr '\0' ' ' <"$f" 2>/dev/null || true)
    case "$cmd" in
      *plumos-joystickd*)
        if [ -z "${expected_opts}" ]; then
          return 0
        fi
        match=1
        for opt in ${expected_opts}; do
          case "$cmd" in
            *"$opt"*) ;;
            *) match=0 ;;
          esac
        done
        [ "${match}" = 1 ] && return 0
        ;;
    esac
  done
  return 1
}

stop_resident_joystickd() {
  for d in /proc/[0-9]*; do
    [ -r "$d/comm" ] || continue
    comm=$(cat "$d/comm" 2>/dev/null || true)
    case "$comm" in
      plumos-joystick*)
        pid=${d#/proc/}
        kill -TERM "${pid}" >/dev/null 2>&1 || true
        ;;
    esac
  done
  sleep 1
  for d in /proc/[0-9]*; do
    [ -r "$d/comm" ] || continue
    comm=$(cat "$d/comm" 2>/dev/null || true)
    case "$comm" in
      plumos-joystick*)
        pid=${d#/proc/}
        kill -KILL "${pid}" >/dev/null 2>&1 || true
        ;;
    esac
  done
}

start_joystickd() {
  case "${PLUMOS_STANDALONE_JOYSTICKD:-auto}" in
    0|no|NO|false|FALSE|none) return 0 ;;
  esac
  [ -x "${PLUMOS_ROOT}/bin/plumos-joystickd" ] || return 0

  joystickd_device_mode=${PLUMOS_STANDALONE_JOYSTICKD_DEVICE_MODE:-xbox}
  case "${joystickd_device_mode}" in
    xbox|gamepad|composite|keyboard|kbd|keys|analog|axes) ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_DEVICE_MODE=${joystickd_device_mode}" >&2; joystickd_device_mode=xbox ;;
  esac
  joystickd_opts="--device-mode ${joystickd_device_mode}"
  case "${PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE:-}" in
    '') ;;
    passthrough|raw|pcsx-menu-l2|pcsx_menu_l2|pcsx|dosbox|dos|digger) joystickd_opts="${joystickd_opts} --keyboard-profile ${PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE}" ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE=${PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE}" >&2 ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}" in
    buttons|button|btn|axes|axis|analog) joystickd_opts="${joystickd_opts} --trigger-mode ${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}" ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE}" >&2 ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-}" in
    '') ;;
    standard|stock|sdl-a30|user|swapped|swap) joystickd_opts="${joystickd_opts} --shoulder-layout ${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT}" ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT}" >&2 ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-}" in
    '') ;;
    axisYL|axisXL|axisYR|axisXR) joystickd_opts="${joystickd_opts} --x-source ${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE}" ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE}" >&2 ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-}" in
    '') ;;
    axisYL|axisXL|axisYR|axisXR) joystickd_opts="${joystickd_opts} --y-source ${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE}" ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE}" >&2 ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_INVERT_X:-0}" in
    1|yes|YES|true|TRUE) joystickd_opts="${joystickd_opts} --invert-x" ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_INVERT_Y:-0}" in
    1|yes|YES|true|TRUE) joystickd_opts="${joystickd_opts} --invert-y" ;;
  esac
  case "${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-}" in
    '') ;;
    mode|guide|thumbl|leftstick|thumbr|rightstick|none|off)
      joystickd_opts="${joystickd_opts} --function-button ${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON}"
      ;;
    *) echo "warning: invalid PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON}" >&2 ;;
  esac

  if find_resident_joystickd "${joystickd_opts}"; then
    return 0
  fi
  if find_resident_joystickd; then
    case "${PLUMOS_STANDALONE_JOYSTICKD_REPLACE_MISMATCH:-1}" in
      0|no|NO|false|FALSE)
        echo "warning: reusing mismatched resident plumos-joystickd" >&2
        return 0
        ;;
      *)
        stop_resident_joystickd
        ;;
    esac
  fi

  "${PLUMOS_ROOT}/bin/plumos-joystickd" ${joystickd_opts} \
    >"${LOG_DIR}/${state_id}-joystickd.log" 2>&1 &
  joystickd_pid=$!
  sleep 1
  if ! kill -0 "${joystickd_pid}" >/dev/null 2>&1; then
    echo "warning: plumos-joystickd exited during startup; see ${LOG_DIR}/${state_id}-joystickd.log" >&2
    joystickd_pid=
  fi
}

stop_owned_joystickd() {
  if [ -n "${joystickd_pid:-}" ] && kill -0 "${joystickd_pid}" >/dev/null 2>&1; then
    kill -TERM "${joystickd_pid}" >/dev/null 2>&1 || true
    kill -KILL "${joystickd_pid}" >/dev/null 2>&1 || true
    wait "${joystickd_pid}" >/dev/null 2>&1 || true
  fi
  joystickd_pid=
}

force_fb_front_page() {
  case "${PLUMOS_STANDALONE_FORCE_FB_FRONT_PAGE:-0}" in
    1|yes|YES|true|TRUE)
      [ -w /sys/class/graphics/fb0/pan ] &&
        echo 0,0 >/sys/class/graphics/fb0/pan 2>/dev/null || true
      ;;
  esac
}

start_fb_front_page_guard() {
  case "${PLUMOS_STANDALONE_FORCE_FB_FRONT_PAGE:-0}" in
    1|yes|YES|true|TRUE) ;;
    *) return 0 ;;
  esac

  force_fb_front_page
  (
    i=0
    while [ "$i" -lt 5 ]; do
      sleep 1
      [ -w /sys/class/graphics/fb0/pan ] &&
        echo 0,0 >/sys/class/graphics/fb0/pan 2>/dev/null || true
      i=$((i + 1))
    done
  ) &
  fb_front_page_guard_pid=$!
}

stop_fb_front_page_guard() {
  if [ -n "${fb_front_page_guard_pid:-}" ] &&
     kill -0 "${fb_front_page_guard_pid}" >/dev/null 2>&1; then
    kill -TERM "${fb_front_page_guard_pid}" >/dev/null 2>&1 || true
    wait "${fb_front_page_guard_pid}" >/dev/null 2>&1 || true
  fi
  fb_front_page_guard_pid=
}

restore_fb() {
  case "${PLUMOS_STANDALONE_RESTORE_FB:-1}" in
    0|no|NO|false|FALSE) return 0 ;;
  esac
  if [ -x "${PLUMOS_ROOT}/bin/plumos-fb-restore" ]; then
    "${PLUMOS_ROOT}/bin/plumos-fb-restore" --quiet >/dev/null 2>&1 || true
    return 0
  fi
  if command -v fbset >/dev/null 2>&1; then
    fbset -g 480 640 480 1280 32 >/dev/null 2>&1 || true
  fi
}

ppsspp_process_running() {
  for d in /proc/[0-9]*; do
    [ -r "$d/cmdline" ] || continue
    cmd=$(tr '\000' ' ' <"$d/cmdline" 2>/dev/null || true)
    case "$cmd" in
      *"/emulators/ppsspp/bin/PPSSPPSDL"*|\
      *"/emulators/ppsspp-vanilla/bin/PPSSPPSDL"*|\
      *"/emulators/ppsspp-display-ui/bin/PPSSPPSDL"*)
        return 0
        ;;
    esac
  done
  return 1
}

reset_ppsspp_instance_counter_if_idle() {
  [ "${ppsspp_instance_counter_cleanup:-0}" = 1 ] || return 0
  case "${PLUMOS_A30_PSP_RESET_INSTANCE_COUNTER:-1}" in
    0|no|NO|false|FALSE) return 0 ;;
  esac
  if ppsspp_process_running; then
    return 0
  fi
  rm -f /dev/shm/PPSSPP_ID /tmp/shm/PPSSPP_ID /run/shm/PPSSPP_ID 2>/dev/null || true
}

cleanup_standalone_temp_paths() {
  [ -n "${standalone_tmp_paths:-}" ] || return 0
  rm -f ${standalone_tmp_paths} 2>/dev/null || true
  standalone_tmp_paths=
}

run_with_fb_restore() {
  child_pid=
  fb_front_page_guard_pid=

  finish_with_fb_restore() {
    rc=$1
    trap - EXIT HUP INT TERM
    # Restore clocks before slower cleanup so an external KILL cannot leave fixed CPU state behind.
    restore_cpu_policy
    stop_fb_front_page_guard
    stop_owned_joystickd
    restore_fb
    reset_ppsspp_instance_counter_if_idle
    cleanup_standalone_temp_paths
    exit "$rc"
  }

  stop_child_and_restore() {
    trap - EXIT HUP INT TERM
    if [ -n "${child_pid:-}" ] && kill -0 "${child_pid}" >/dev/null 2>&1; then
      kill -TERM "${child_pid}" >/dev/null 2>&1 || true
      kill -KILL "${child_pid}" >/dev/null 2>&1 || true
    fi
    restore_cpu_policy
    stop_fb_front_page_guard
    stop_owned_joystickd
    if [ -n "${child_pid:-}" ] && kill -0 "${child_pid}" >/dev/null 2>&1; then
      wait "${child_pid}" >/dev/null 2>&1 || true
    fi
    restore_fb
    reset_ppsspp_instance_counter_if_idle
    cleanup_standalone_temp_paths
    exit 143
  }

  restore_fb
  trap 'finish_with_fb_restore $?' EXIT
  trap 'stop_child_and_restore' HUP INT TERM
  "${LOADER}" --library-path "${LIB_PATH}" "$@" &
  child_pid=$!
  start_fb_front_page_guard
  wait "${child_pid}"
  rc=$?
  child_pid=
  finish_with_fb_restore "$rc"
}

ensure_ppsspp_pause_menu_mapping() {
  controls_file=$1
  [ -s "${controls_file}" ] || return 0
  pause_mapping=${PLUMOS_A30_PSP_PAUSE_MAPPING:-10-4}

  tmp="${controls_file}.tmp.$$"
  awk -v pause_mapping="${pause_mapping}" '
    BEGIN { done = 0 }
    /^Pause[[:space:]]*=/ {
      $0 = "Pause = " pause_mapping
      done = 1
    }
    { print }
    END {
      if (!done) {
        print "Pause = " pause_mapping
      }
    }
  ' "${controls_file}" >"${tmp}" && mv "${tmp}" "${controls_file}"
  rm -f "${tmp}"
}

configure_ppsspp_menu_button() {
  ppsspp_menu_button=${PLUMOS_A30_PSP_MENU_BUTTON:-function}
  case "${ppsspp_menu_button}" in
    home|function|func|guide|mode|back)
      export PLUMOS_A30_PSP_MENU_BUTTON=function
      export PLUMOS_A30_PSP_PAUSE_MAPPING=10-4
      export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=mode
      ;;
    l2|L2|lefttrigger|left_trigger)
      export PLUMOS_A30_PSP_MENU_BUTTON=l2
      export PLUMOS_A30_PSP_PAUSE_MAPPING=10-104
      export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=none
      ;;
    r2|R2|righttrigger|right_trigger)
      export PLUMOS_A30_PSP_MENU_BUTTON=r2
      export PLUMOS_A30_PSP_PAUSE_MAPPING=10-105
      export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=none
      ;;
    none|off|disabled|0|false)
      export PLUMOS_A30_PSP_MENU_BUTTON=none
      export PLUMOS_A30_PSP_PAUSE_MAPPING=
      export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=none
      ;;
    *)
      echo "warning: invalid PLUMOS_A30_PSP_MENU_BUTTON=${ppsspp_menu_button}; using function" >&2
      export PLUMOS_A30_PSP_MENU_BUTTON=function
      export PLUMOS_A30_PSP_PAUSE_MAPPING=10-4
      export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=mode
      ;;
  esac
}

sync_ppsspp_pause_menu_mapping_files() {
  controls_dir="${XDG_CONFIG_HOME}/ppsspp/PSP/SYSTEM"
  controls_file="${controls_dir}/controls.ini"
  legacy_controls_dir="${HOME}/.config/ppsspp/PSP/SYSTEM"
  legacy_controls_file="${legacy_controls_dir}/controls.ini"

  if [ -s "${controls_file}" ]; then
    ensure_ppsspp_pause_menu_mapping "${controls_file}"
  fi
  if [ "${legacy_controls_file}" != "${controls_file}" ] && [ -s "${legacy_controls_file}" ]; then
    ensure_ppsspp_pause_menu_mapping "${legacy_controls_file}"
  fi
}

ensure_ppsspp_controls_config() {
  case "${PLUMOS_A30_PSP_CONTROLS:-none}" in
    none|0|no|NO|false|FALSE) return 0 ;;
  esac

  controls_dir="${XDG_CONFIG_HOME}/ppsspp/PSP/SYSTEM"
  controls_file="${controls_dir}/controls.ini"
  legacy_controls_dir="${HOME}/.config/ppsspp/PSP/SYSTEM"
  legacy_controls_file="${legacy_controls_dir}/controls.ini"
  force=0
  case "${PLUMOS_A30_PSP_FORCE_CONTROLS:-0}" in
    1|yes|YES|true|TRUE) force=1 ;;
  esac
  if [ "${force}" != 1 ] && [ ! -s "${controls_file}" ] && [ -s "${legacy_controls_file}" ]; then
    mkdir -p "${controls_dir}"
    cp "${legacy_controls_file}" "${controls_file}"
  fi
  if [ "${force}" != 1 ] && [ -s "${controls_file}" ]; then
    ensure_ppsspp_pause_menu_mapping "${controls_file}"
    if [ "${legacy_controls_file}" != "${controls_file}" ]; then
      mkdir -p "${legacy_controls_dir}"
      if [ -s "${legacy_controls_file}" ]; then
        ensure_ppsspp_pause_menu_mapping "${legacy_controls_file}"
      else
        cp "${controls_file}" "${legacy_controls_file}"
      fi
    fi
    return 0
  fi

  mkdir -p "${controls_dir}"
  cat >"${controls_file}" <<'PPSSPP_CONTROLS_EOF'
[ControlMapping]
Up = 1-19,10-19
Down = 1-20,10-20
Left = 1-21,10-21
Right = 1-22,10-22
Circle = 1-52,10-190
Cross = 1-54,10-189
Square = 1-29,10-191
Triangle = 1-47,10-188
Start = 1-62,10-197
Select = 1-66,10-196
L = 1-45,10-193
R = 1-51,10-192
An.Up = 1-37,10-4003
An.Down = 1-39,10-4002
An.Left = 1-38,10-4001
An.Right = 1-40,10-4000
Analog limiter = 1-60
RapidFire = 1-59
Fast-forward = 1-61
Pause = 10-4
Pause (no menu) = 1-138
SpeedToggle = 1-68
Analog speed = 10-4036
Rewind = 1-67
Toggle Debugger = 1-142
PPSSPP_CONTROLS_EOF
  if [ "${legacy_controls_file}" != "${controls_file}" ]; then
    mkdir -p "${legacy_controls_dir}"
    cp "${controls_file}" "${legacy_controls_file}"
  fi
}

write_ppsspp_profile_config() {
  profile_arg=
  profile_cfg="${STATE_DIR}/config/plumos-a30-ppsspp-profile.ini"
  case "${PLUMOS_A30_PSP_APPEND_PROFILE:-0}" in
    1|yes|YES|true|TRUE|on|ON)
      ;;
    *)
      rm -f "${profile_cfg}"
      return 0
      ;;
  esac

  case "${PLUMOS_A30_PSP_PROFILE:-standard}" in
    none|0|no|NO|false|FALSE) return 0 ;;
    standard)
      ;;
    *)
      echo "warning: unknown PLUMOS_A30_PSP_PROFILE=${PLUMOS_A30_PSP_PROFILE}; using standard profile" >&2
      ;;
  esac

  ppsspp_ui_scale="${PLUMOS_A30_PSP_UI_SCALE:--6}"
  cat >"${profile_cfg}" <<PPSSPP_PROFILE_EOF
[General]
UIScaleFactor = ${ppsspp_ui_scale}

[Sound]
Enable = True
ExtraAudioBuffering = True
AudioBufferSize = 1024
FillAudioGaps = True
AudioSyncMode = 0

[Graphics]
InternalResolution = 1
FrameSkip = 0
AutoFrameSkip = False
AnisotropyLevel = 0
MultiSampleLevel = 0
SplineBezierQuality = 0
LowLatencyPresent = False
PPSSPP_PROFILE_EOF

  case "${PLUMOS_A30_PSP_LAYOUT:-a30-landscape}" in
    none|0|no|NO|false|FALSE)
      ;;
    a30-landscape|landscape-psp)
      cat >>"${profile_cfg}" <<'PPSSPP_LAYOUT_EOF'

[DisplayLayout.Landscape]
InternalScreenRotation = 1
RotateControlsWithScreen = False
DisplayStretch = False
DisplayOffsetX = 0.500000
DisplayOffsetY = 0.500000
DisplayScale = 1.000000
DisplayIntegerScale = False
DisplayAspectRatio = 1.000000
IgnoreScreenInsets = True

[DisplayLayout.Portrait]
InternalScreenRotation = 2
RotateControlsWithScreen = False
DisplayStretch = False
DisplayOffsetX = 0.500000
DisplayOffsetY = 0.500000
DisplayScale = 1.000000
DisplayIntegerScale = False
DisplayAspectRatio = 1.000000
IgnoreScreenInsets = True
PPSSPP_LAYOUT_EOF
      ;;
    portrait-vertical)
      cat >>"${profile_cfg}" <<'PPSSPP_LAYOUT_EOF'

[DisplayLayout.Portrait]
InternalScreenRotation = 2
RotateControlsWithScreen = False
DisplayStretch = False
DisplayOffsetX = 0.500000
DisplayOffsetY = 0.500000
DisplayScale = 1.000000
DisplayIntegerScale = False
DisplayAspectRatio = 1.000000
IgnoreScreenInsets = True
PPSSPP_LAYOUT_EOF
      ;;
    *)
      echo "warning: unknown PLUMOS_A30_PSP_LAYOUT=${PLUMOS_A30_PSP_LAYOUT}; using no layout override" >&2
      ;;
  esac
  profile_arg="--appendconfig=${profile_cfg}"
}

scummvm_has_config_arg() {
  for arg in "$@"; do
    case "${arg}" in
      --config|--config=*) return 0 ;;
    esac
  done
  return 1
}

scummvm_valid_target_id() {
  case "${1:-}" in
    ""|*[!A-Za-z0-9._-]*) return 1 ;;
    *) return 0 ;;
  esac
}

scummvm_target_from_file() {
  sidecar=$1
  [ -f "${sidecar}" ] || return 1

  while IFS= read -r line || [ -n "${line}" ]; do
    line=$(printf '%s' "${line}" | tr -d '\r' | sed 's/#.*$//; s/^[[:space:]]*//; s/[[:space:]]*$//')
    [ -n "${line}" ] || continue
    case "${line}" in
      target=*) line=${line#target=} ;;
      gameid=*) line=${line#gameid=} ;;
      scummvm_target=*) line=${line#scummvm_target=} ;;
      \"target\"*) line=$(printf '%s' "${line}" | sed 's/.*"target"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/') ;;
    esac
    line=$(printf '%s' "${line}" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//; s/^"//; s/"$//; s/^'\''//; s/'\''$//')
    if scummvm_valid_target_id "${line}"; then
      printf '%s\n' "${line}"
      return 0
    fi
    echo "warning: ignoring invalid ScummVM target id in ${sidecar}: ${line}" >&2
    return 1
  done <"${sidecar}"

  return 1
}

resolve_scummvm_directory_target() {
  game_dir=$1
  base=${game_dir##*/}
  parent=${game_dir%/*}

  for sidecar in \
    "${game_dir}/.plumos-scummvm-target" \
    "${game_dir}/scummvm-target.txt" \
    "${game_dir}/.scummvm" \
    "${parent}/${base}.scummvm" \
    "${parent}/${base}.svm"; do
    if target=$(scummvm_target_from_file "${sidecar}"); then
      printf '%s\n' "${target}"
      return 0
    fi
  done

  return 1
}

ensure_scummvm_config() {
  scummvm_config_arg=
  case "${PLUMOS_A30_SCUMMVM_DEFAULT_CONFIG:-1}" in
    0|no|NO|false|FALSE|off|OFF) return 0 ;;
  esac
  if scummvm_has_config_arg "$@"; then
    return 0
  fi

  scummvm_config=${PLUMOS_A30_SCUMMVM_CONFIG:-${STATE_DIR}/config/scummvm.ini}
  if [ ! -s "${scummvm_config}" ]; then
    mkdir -p "$(dirname "${scummvm_config}")"
    cat >"${scummvm_config}" <<SCUMMVM_CONFIG_EOF
[scummvm]
rotation_mode=${PLUMOS_A30_SCUMMVM_ROTATION:-270}
gui_theme=${PLUMOS_A30_SCUMMVM_GUI_THEME:-scummmodern-a30-md}
gfx_mode=surfacesdl
fullscreen=${PLUMOS_A30_SCUMMVM_FULLSCREEN:-true}
aspect_ratio=${PLUMOS_A30_SCUMMVM_ASPECT_RATIO:-true}
stretch_mode=${PLUMOS_A30_SCUMMVM_STRETCH_MODE:-fit}
filtering=${PLUMOS_A30_SCUMMVM_FILTERING:-false}
vsync=${PLUMOS_A30_SCUMMVM_VSYNC:-true}
SCUMMVM_CONFIG_EOF
  fi
  scummvm_config_arg="--config=${scummvm_config}"
}

ppsspp_escape_exit_arg() {
  case "${PLUMOS_A30_PSP_ESCAPE_EXIT:-0}" in
    1|yes|YES|true|TRUE) echo "--escape-exit" ;;
  esac
}

find_unzip_command() {
  if [ -x "${PLUMOS_ROOT}/bin/unzip" ]; then
    printf '%s\n' "${PLUMOS_ROOT}/bin/unzip"
    return 0
  fi
  command -v unzip 2>/dev/null || return 1
}

red_viper_zip_member() {
  unzip_cmd=$1
  zip_path=$2
  "${unzip_cmd}" -l -q "${zip_path}" 2>/dev/null |
    awk '
      {
        name = $0
        sub(/\r$/, "", name)
        sub(/^[[:space:]]*[0-9]+[[:space:]]+[0-9-]+[[:space:]]+[0-9:]+[[:space:]]+/, "", name)
        lower = tolower(name)
        if (lower ~ /\.(vb|vboy|bin)$/) {
          print name
          exit
        }
      }
    '
}

prepare_red_viper_rom() {
  red_viper_original_rom=$1
  red_viper_prepared_rom=${red_viper_original_rom}
  red_viper_save_base=${red_viper_original_rom##*/}
  red_viper_ext=${red_viper_original_rom##*.}
  red_viper_ext=$(printf '%s' "${red_viper_ext}" | tr 'A-Z' 'a-z')

  case "${red_viper_ext}" in
    vb|vboy|bin)
      return 0
      ;;
    zip)
      unzip_cmd=$(find_unzip_command) || {
        echo "error: unzip is required for Red Viper zip ROMs" >&2
        exit 127
      }
      red_viper_member=$(red_viper_zip_member "${unzip_cmd}" "${red_viper_original_rom}")
      if [ -z "${red_viper_member}" ]; then
        echo "error: Red Viper zip has no .vb/.vboy/.bin member: ${red_viper_original_rom}" >&2
        exit 2
      fi
      mkdir -p "${STATE_DIR}/tmp"
      red_viper_tmp="${STATE_DIR}/tmp/red-viper-rom-$$.vb"
      if ! "${unzip_cmd}" -p "${red_viper_original_rom}" "${red_viper_member}" >"${red_viper_tmp}"; then
        rm -f "${red_viper_tmp}"
        echo "error: failed to extract Red Viper ROM member: ${red_viper_member}" >&2
        exit 1
      fi
      red_viper_prepared_rom=${red_viper_tmp}
      standalone_tmp_paths="${standalone_tmp_paths} ${red_viper_tmp}"
      ;;
    7z)
      echo "error: Red Viper 7z ROMs are not supported on this A30 image; use .vb/.vboy/.bin or .zip" >&2
      exit 2
      ;;
    *)
      echo "error: unsupported Red Viper ROM extension: ${red_viper_original_rom}" >&2
      exit 2
      ;;
  esac
}

apply_system_volume

case "${id}" in
  ppsspp)
    ppsspp_instance_counter_cleanup=1
    cd "${EMU_ROOT}/ppsspp" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}
    export PLUMOS_A30_DISPLAY_ROTATION=${PLUMOS_A30_DISPLAY_ROTATION:-ccw}
    export PLUMOS_A30_DISPLAY_SWAP=${PLUMOS_A30_DISPLAY_SWAP:-0}
    export PLUMOS_A30_DISPLAY_LOGICAL=${PLUMOS_A30_DISPLAY_LOGICAL:-854x480}
    export PLUMOS_A30_DISPLAY_OUTPUT=${PLUMOS_A30_DISPLAY_OUTPUT:-}
    export PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=${PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE:-1}
    export PLUMOS_A30_UI_ROTATION=${PLUMOS_A30_UI_ROTATION:-none}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_PSP_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_PSP_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-standard}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    configure_ppsspp_menu_button
    cpu_policy=${PLUMOS_A30_PSP_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_PSP_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_PSP_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    sync_ppsspp_pause_menu_mapping_files
    ensure_ppsspp_controls_config
    write_ppsspp_profile_config
    escape_exit_arg=$(ppsspp_escape_exit_arg)
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    reset_ppsspp_instance_counter_if_idle
    run_with_fb_restore "${EMU_ROOT}/ppsspp/bin/PPSSPPSDL" --fullscreen ${escape_exit_arg:+"${escape_exit_arg}"} --graphics=gles ${profile_arg:+"${profile_arg}"} "$@"
    ;;
  ppsspp-vanilla)
    ppsspp_instance_counter_cleanup=1
    cd "${EMU_ROOT}/ppsspp-vanilla" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_PSP_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_PSP_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-standard}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_A30_PSP_JOYSTICKD_FUNCTION_BUTTON:-${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-none}}
    cpu_policy=${PLUMOS_A30_PSP_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_PSP_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_PSP_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    escape_exit_arg=$(ppsspp_escape_exit_arg)
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    reset_ppsspp_instance_counter_if_idle
    run_with_fb_restore "${EMU_ROOT}/ppsspp-vanilla/bin/PPSSPPSDL" --fullscreen ${escape_exit_arg:+"${escape_exit_arg}"} --graphics=gles "$@"
    ;;
  ppsspp-display-ui)
    ppsspp_instance_counter_cleanup=1
    cd "${EMU_ROOT}/ppsspp-display-ui" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}
    export PLUMOS_A30_DISPLAY_ROTATION=${PLUMOS_A30_DISPLAY_ROTATION:-ccw}
    export PLUMOS_A30_DISPLAY_SWAP=${PLUMOS_A30_DISPLAY_SWAP:-0}
    export PLUMOS_A30_DISPLAY_LOGICAL=${PLUMOS_A30_DISPLAY_LOGICAL:-854x480}
    export PLUMOS_A30_DISPLAY_OUTPUT=${PLUMOS_A30_DISPLAY_OUTPUT:-}
    export PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=${PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE:-1}
    export PLUMOS_A30_UI_ROTATION=${PLUMOS_A30_UI_ROTATION:-none}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_PSP_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_PSP_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-standard}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_PSP_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_A30_PSP_JOYSTICKD_FUNCTION_BUTTON:-${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-none}}
    cpu_policy=${PLUMOS_A30_PSP_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_PSP_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_PSP_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    write_ppsspp_profile_config
    escape_exit_arg=$(ppsspp_escape_exit_arg)
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    reset_ppsspp_instance_counter_if_idle
    run_with_fb_restore "${EMU_ROOT}/ppsspp-display-ui/bin/PPSSPPSDL" --fullscreen ${escape_exit_arg:+"${escape_exit_arg}"} --graphics=gles ${profile_arg:+"${profile_arg}"} "$@"
    ;;
  scummvm)
    cd "${EMU_ROOT}/scummvm" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_SCUMMVM_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_SCUMMVM_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-user}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_SCUMMVM_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_SCUMMVM_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_A30_SCUMMVM_JOYSTICKD_FUNCTION_BUTTON:-${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-none}}
    DATA_DIR=${EMU_ROOT}/scummvm/share/scummvm
    cpu_policy=${PLUMOS_A30_SCUMMVM_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_SCUMMVM_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1200000}}
    cpu_cores=${PLUMOS_A30_SCUMMVM_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-2}}
    if [ "$#" -eq 1 ] && [ -d "$1" ]; then
      scummvm_game_dir=$1
      if [ -n "${PLUMOS_A30_SCUMMVM_TARGET:-}" ]; then
        scummvm_target=${PLUMOS_A30_SCUMMVM_TARGET}
      elif scummvm_target=$(resolve_scummvm_directory_target "${scummvm_game_dir}"); then
        :
      else
        scummvm_target=sky
      fi
      if ! scummvm_valid_target_id "${scummvm_target}"; then
        echo "error: invalid ScummVM target id: ${scummvm_target}" >&2
        exit 2
      fi
      set -- "--path=${scummvm_game_dir}" "${scummvm_target}"
    fi
    ensure_scummvm_config "$@"
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    run_with_fb_restore "${EMU_ROOT}/scummvm/bin/scummvm" ${scummvm_config_arg:+"${scummvm_config_arg}"} --themepath="${DATA_DIR}" --extrapath="${DATA_DIR}" "$@"
    ;;
  easyrpg)
    easyrpg_project_path=
    if [ "$#" -eq 1 ]; then
      case "$1" in
        -*)
          ;;
        *)
          easyrpg_project_path=$1
          shift
          ;;
      esac
    fi
    cd "${STATE_DIR}" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export PLUMOS_A30_EASYRPG_ROTATION=${PLUMOS_A30_EASYRPG_ROTATION:-ccw}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_EASYRPG_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_EASYRPG_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-user}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_EASYRPG_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_EASYRPG_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_A30_EASYRPG_JOYSTICKD_FUNCTION_BUTTON:-${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-none}}
    cpu_policy=${PLUMOS_A30_EASYRPG_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_EASYRPG_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-648000}}
    cpu_cores=${PLUMOS_A30_EASYRPG_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-2}}
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    if [ -n "${easyrpg_project_path}" ]; then
      run_with_fb_restore "${EMU_ROOT}/easyrpg/bin/easyrpg-player" --fullscreen --project-path "${easyrpg_project_path}" "$@"
    else
      run_with_fb_restore "${EMU_ROOT}/easyrpg/bin/easyrpg-player" --fullscreen "$@"
    fi
    ;;
  openbor)
    openbor_pak=
    if [ "$#" -gt 0 ]; then
      case "$1" in
        -*)
          ;;
        *)
          openbor_pak=$1
          shift
          ;;
      esac
    fi
    if [ -z "${openbor_pak}" ]; then
      echo "error: OpenBOR requires a .pak path" >&2
      exit 2
    fi
    if [ ! -f "${openbor_pak}" ]; then
      echo "error: OpenBOR .pak not found: ${openbor_pak}" >&2
      exit 2
    fi
    case "${openbor_pak}" in
      *.pak|*.PAK|*.Pak)
        ;;
      *)
        echo "error: unsupported OpenBOR extension: ${openbor_pak}" >&2
        exit 2
        ;;
    esac
    cd "${STATE_DIR}" || exit 1
    mkdir -p Paks Saves Logs ScreenShots
    export PLUMOS_A30_OPENBOR_PAK="${openbor_pak}"
    case "${PLUMOS_STANDALONE_USE_STOCK_SDL:-1}" in
      0|no|NO|false|FALSE)
        openbor_default_video_driver=dummy
        openbor_default_rotation=none
        openbor_default_force_front_page=0
        openbor_default_renderer_opengl=0
        openbor_default_render_driver=
        openbor_default_direct_present=1
        ;;
      *)
        openbor_default_video_driver=mali
        openbor_default_rotation=ccw
        openbor_default_force_front_page=1
        openbor_default_renderer_opengl=0
        openbor_default_render_driver=
        openbor_default_direct_present=0
        ;;
    esac
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-${openbor_default_video_driver}}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}
    if [ -z "${SDL_RENDER_DRIVER:-}" ] && [ -n "${openbor_default_render_driver}" ]; then
      export SDL_RENDER_DRIVER=${openbor_default_render_driver}
    fi
    export PLUMOS_A30_OPENBOR_RENDERER_OPENGL=${PLUMOS_A30_OPENBOR_RENDERER_OPENGL:-${openbor_default_renderer_opengl}}
    export PLUMOS_A30_OPENBOR_DIRECT_PRESENT=${PLUMOS_A30_OPENBOR_DIRECT_PRESENT:-${openbor_default_direct_present}}
    export PLUMOS_A30_OPENBOR_DIRECT_ROTATION=${PLUMOS_A30_OPENBOR_DIRECT_ROTATION:-ccw}
    export PLUMOS_A30_OPENBOR_DIRECT_LOGICAL_SIZE=${PLUMOS_A30_OPENBOR_DIRECT_LOGICAL_SIZE:-640x480}
    export PLUMOS_A30_OPENBOR_DIRECT_BGR565=${PLUMOS_A30_OPENBOR_DIRECT_BGR565:-1}
    export PLUMOS_A30_OPENBOR_DIRECT_LINEAR=${PLUMOS_A30_OPENBOR_DIRECT_LINEAR:-0}
    export PLUMOS_A30MALI_ROTATION=${PLUMOS_A30MALI_ROTATION:-cw}
    export PLUMOS_A30MALI_LOGICAL_SIZE=${PLUMOS_A30MALI_LOGICAL_SIZE:-640x480}
    export PLUMOS_A30MALI_SHADER_FIT=${PLUMOS_A30MALI_SHADER_FIT:-1}
    export PLUMOS_A30_OPENBOR_ROTATION=${PLUMOS_A30_OPENBOR_ROTATION:-${openbor_default_rotation}}
    export PLUMOS_A30_OPENBOR_SCALE=${PLUMOS_A30_OPENBOR_SCALE:-fit}
    export PLUMOS_A30_OPENBOR_FULLSCREEN=${PLUMOS_A30_OPENBOR_FULLSCREEN:-1}
    export PLUMOS_STANDALONE_FORCE_FB_FRONT_PAGE=${PLUMOS_A30_OPENBOR_FORCE_FB_FRONT_PAGE:-${openbor_default_force_front_page}}
    export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-030003f05e0400008e0200005e040000,plumOS A30 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b8,guide:b10,start:b9,leftstick:b6,rightstick:b7,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,}"
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_OPENBOR_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_OPENBOR_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-user}}
    export PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE=${PLUMOS_A30_OPENBOR_JOYSTICKD_X_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_X_SOURCE:-axisYR}}
    export PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE=${PLUMOS_A30_OPENBOR_JOYSTICKD_Y_SOURCE:-${PLUMOS_STANDALONE_JOYSTICKD_Y_SOURCE:-axisXR}}
    export PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON=${PLUMOS_A30_OPENBOR_JOYSTICKD_FUNCTION_BUTTON:-${PLUMOS_STANDALONE_JOYSTICKD_FUNCTION_BUTTON:-none}}
    cpu_policy=${PLUMOS_A30_OPENBOR_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_OPENBOR_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_OPENBOR_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    run_with_fb_restore "${EMU_ROOT}/openbor/bin/OpenBOR" "$@"
    ;;
  red_viper|red-viper)
    if [ "$#" -lt 1 ]; then
      echo "error: Red Viper requires a ROM path" >&2
      exit 2
    fi
    cd "${EMU_ROOT}/red_viper" || exit 1
    red_viper_rom_arg=$1
    shift
    prepare_red_viper_rom "${red_viper_rom_arg}"
    cpu_policy=${PLUMOS_A30_RED_VIPER_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_RED_VIPER_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-648000}}
    cpu_cores=${PLUMOS_A30_RED_VIPER_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-2}}
    red_viper_renderer=${PLUMOS_A30_RED_VIPER_RENDERER:-sdlgl}
    case "${red_viper_renderer}" in
      sdlgl|stock-sdl|stock_sdl|gles|mali)
        if [ ! -x "${EMU_ROOT}/red_viper/bin/red-viper-sdlgl-a30" ]; then
          echo "warning: Red Viper SDL/GLES binary is missing; falling back to fbdev renderer" >&2
          red_viper_renderer=fbdev
        fi
        ;;
    esac
    export PLUMOS_A30_RED_VIPER_EYE=${PLUMOS_A30_RED_VIPER_EYE:-both}
    export PLUMOS_A30_RED_VIPER_COLOR=${PLUMOS_A30_RED_VIPER_COLOR:-red}
    export PLUMOS_A30_RED_VIPER_WAIT_VSYNC=${PLUMOS_A30_RED_VIPER_WAIT_VSYNC:-1}
    export PLUMOS_A30_RED_VIPER_FRAME_SKIP=${PLUMOS_A30_RED_VIPER_FRAME_SKIP:-0}
    export PLUMOS_A30_RED_VIPER_FAST_FORWARD=${PLUMOS_A30_RED_VIPER_FAST_FORWARD:-0}
    export PLUMOS_A30_RED_VIPER_FF_TOGGLE=${PLUMOS_A30_RED_VIPER_FF_TOGGLE:-0}
    export PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK=${PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK:-0}
    export PLUMOS_A30_RED_VIPER_PERF_INFO=${PLUMOS_A30_RED_VIPER_PERF_INFO:-0}
    export PLUMOS_A30_RED_VIPER_AUDIO=${PLUMOS_A30_RED_VIPER_AUDIO:-alsa}
    export PLUMOS_A30_RED_VIPER_ALSA_DEVICE=${PLUMOS_A30_RED_VIPER_ALSA_DEVICE:-default}
    export PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US=${PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US:-160000}
    export PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS=${PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS:-6}
    export PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS=${PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS:-8}
    export PLUMOS_A30_RED_VIPER_AUDIO_GAP=${PLUMOS_A30_RED_VIPER_AUDIO_GAP:-fade}
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    case "${red_viper_renderer}" in
      sdlgl|stock-sdl|stock_sdl|gles|mali)
        export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
        export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
        export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=${SDL_AUDIO_ALSA_SET_BUFFER_SIZE:-1}
        export PLUMOS_A30_RED_VIPER_SDLGL_ROTATION=${PLUMOS_A30_RED_VIPER_SDLGL_ROTATION:-ccw}
        export PLUMOS_A30_RED_VIPER_SDLGL_SCALE=${PLUMOS_A30_RED_VIPER_SDLGL_SCALE:-${PLUMOS_A30_RED_VIPER_SCALE:-fit}}
        export PLUMOS_A30_RED_VIPER_SDLGL_PHYSICAL=${PLUMOS_A30_RED_VIPER_SDLGL_PHYSICAL:-480x640}
        run_with_fb_restore "${EMU_ROOT}/red_viper/bin/red-viper-sdlgl-a30" \
          "${red_viper_prepared_rom}" "$@"
        ;;
      fbdev|software|cpu)
        run_with_fb_restore "${EMU_ROOT}/red_viper/bin/red-viper-a30" \
          --fb "${PLUMOS_A30_RED_VIPER_FB:-/dev/fb0}" \
          --input "${PLUMOS_A30_RED_VIPER_INPUT:-/dev/input/event3}" \
          --rotation "${PLUMOS_A30_RED_VIPER_ROTATION:-ccw}" \
          --scale "${PLUMOS_A30_RED_VIPER_SCALE:-fit}" \
          --eye "${PLUMOS_A30_RED_VIPER_EYE}" \
          --color "${PLUMOS_A30_RED_VIPER_COLOR}" \
          --audio "${PLUMOS_A30_RED_VIPER_AUDIO}" \
          --save-base "${red_viper_save_base}" \
          "${red_viper_prepared_rom}" "$@"
        ;;
      *)
        echo "error: invalid PLUMOS_A30_RED_VIPER_RENDERER=${red_viper_renderer}" >&2
        exit 2
        ;;
    esac
    ;;
  dosbox-staging|dosbox)
    cd "${EMU_ROOT}/dosbox-staging" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export PLUMOS_A30_DOSBOX_ROTATION=${PLUMOS_A30_DOSBOX_ROTATION:-ccw}
    cpu_policy=${PLUMOS_A30_DOSBOX_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_DOSBOX_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_DOSBOX_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    run_with_fb_restore "${EMU_ROOT}/dosbox-staging/bin/dosbox" -noconsole -fullscreen "$@"
    ;;
  pcsx_rearmed|pcsx)
    cd "${EMU_ROOT}/pcsx_rearmed" || exit 1
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-fbcon}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export PLUMOS_A30_PCSX_SAFE_FB=${PLUMOS_A30_PCSX_SAFE_FB:-1}
    export PLUMOS_A30_PCSX_SCALE2X=${PLUMOS_A30_PCSX_SCALE2X:-1}
    export PLUMOS_A30_PCSX_NATIVE_FB32=${PLUMOS_A30_PCSX_NATIVE_FB32:-1}
    export PLUMOS_A30_PCSX_ROTATION=${PLUMOS_A30_PCSX_ROTATION:-ccw}
    export PLUMOS_A30_PCSX_MENU_SCALE=${PLUMOS_A30_PCSX_MENU_SCALE:-2}
    export PLUMOS_A30_PCSX_MENU_ZOOM=${PLUMOS_A30_PCSX_MENU_ZOOM:-1}
    export PLUMOS_A30_PCSX_MENU_EDGE_MASK=${PLUMOS_A30_PCSX_MENU_EDGE_MASK:-0}
    export PLUMOS_A30_PCSX_MENU_LANDSCAPE=${PLUMOS_A30_PCSX_MENU_LANDSCAPE:-1}
    export PLUMOS_A30_PSX_BIOS_DIR=${PLUMOS_A30_PSX_BIOS_DIR:-/mnt/SDCARD/Bios}
    export PLUMOS_STANDALONE_JOYSTICKD_DEVICE_MODE=${PLUMOS_A30_PSX_JOYSTICKD_DEVICE_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_DEVICE_MODE:-keyboard}}
    export PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE=${PLUMOS_A30_PSX_JOYSTICKD_KEYBOARD_PROFILE:-${PLUMOS_STANDALONE_JOYSTICKD_KEYBOARD_PROFILE:-passthrough}}
    export PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE=${PLUMOS_A30_PSX_JOYSTICKD_TRIGGER_MODE:-${PLUMOS_STANDALONE_JOYSTICKD_TRIGGER_MODE:-buttons}}
    export PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT=${PLUMOS_A30_PSX_JOYSTICKD_SHOULDER_LAYOUT:-${PLUMOS_STANDALONE_JOYSTICKD_SHOULDER_LAYOUT:-user}}
    cpu_policy=${PLUMOS_A30_PSX_CPU_POLICY:-${PLUMOS_STANDALONE_CPU_POLICY:-fixed}}
    cpu_freq=${PLUMOS_A30_PSX_CPU_FREQ:-${PLUMOS_STANDALONE_CPU_FREQ:-1344000}}
    cpu_cores=${PLUMOS_A30_PSX_CPU_CORES:-${PLUMOS_STANDALONE_CPU_CORES:-4}}
    if [ "$#" -gt 0 ]; then
      case "$1" in
        -*)
          ;;
        *)
          rom=$1
          shift
          set -- -cdfile "${rom}" "$@"
        ;;
      esac
    fi
    apply_cpu_policy "${cpu_policy}" "${cpu_freq}" "${cpu_cores}"
    start_joystickd
    run_with_fb_restore "${EMU_ROOT}/pcsx_rearmed/bin/pcsx" "$@"
    ;;
  *)
    echo "error: unknown standalone emulator: ${id}" >&2
    usage >&2
    exit 2
    ;;
esac
EOF
  chmod 0755 "${launcher}"
}

write_standalone_config_defaults() {
  local config_dir="${TARGET_DIR}/plumos/config/standalone"

  mkdir -p "${config_dir}"
  cat >"${config_dir}/ppsspp.env" <<'EOF'
# PPSSPP launcher overrides for Miyoo A30.
# This file is user-mutable and is preserved by scripts/deploy-a30.sh.

PLUMOS_STANDALONE_USE_STOCK_SDL=1
PLUMOS_STOCK_LIB=/mnt/SDCARD/plumos/emulators/ppsspp/lib:/mnt/SDCARD/miyoo/lib:/usr/miyoo/lib

PLUMOS_A30_PSP_CPU_POLICY=fixed
PLUMOS_A30_PSP_CPU_FREQ=1344000
PLUMOS_A30_PSP_CPU_CORES=4

PLUMOS_A30_DISPLAY_ROTATION=ccw
PLUMOS_A30_DISPLAY_SWAP=0
PLUMOS_A30_DISPLAY_LOGICAL=854x480
PLUMOS_A30_DISPLAY_OUTPUT=
PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=1
PLUMOS_A30_UI_ROTATION=none

PLUMOS_A30_PSP_MENU_BUTTON=function
PLUMOS_A30_PSP_PAUSE_MAPPING=10-4
PLUMOS_A30_PSP_JOYSTICKD_TRIGGER_MODE=buttons
PLUMOS_A30_PSP_JOYSTICKD_SHOULDER_LAYOUT=standard
PLUMOS_A30_PSP_JOYSTICKD_X_SOURCE=axisYR
PLUMOS_A30_PSP_JOYSTICKD_Y_SOURCE=axisXR
PLUMOS_A30_PSP_JOYSTICKD_FUNCTION_BUTTON=mode

PLUMOS_A30_PSP_CONTROLS=none
PLUMOS_A30_PSP_FORCE_CONTROLS=0

PLUMOS_A30_PSP_APPEND_PROFILE=0
PLUMOS_A30_PSP_ESCAPE_EXIT=0
PLUMOS_A30_PSP_RESET_INSTANCE_COUNTER=1
EOF
  append_manifest
  append_manifest "config=plumos/config/standalone/ppsspp.env"

  cat >"${config_dir}/pcsx_rearmed.env" <<'EOF'
# PCSX-ReARMed launcher overrides for Miyoo A30.
# This file is user-mutable and is preserved by scripts/deploy-a30.sh.

PLUMOS_A30_PSX_JOYSTICKD_DEVICE_MODE=keyboard
PLUMOS_A30_PSX_BIOS_DIR=/mnt/SDCARD/Bios
# L2-as-menu is paused; use the emulator-side Function menu binding.
# Set this to pcsx-menu-l2 to re-enable the L2 menu shortcut.
PLUMOS_A30_PSX_JOYSTICKD_KEYBOARD_PROFILE=passthrough
PLUMOS_A30_PSX_JOYSTICKD_TRIGGER_MODE=buttons
PLUMOS_A30_PSX_JOYSTICKD_SHOULDER_LAYOUT=user
EOF
  append_manifest "config=plumos/config/standalone/pcsx_rearmed.env"

  cat >"${config_dir}/openbor.env" <<'EOF'
# OpenBOR launcher overrides for Miyoo A30.
# This file is user-mutable and is preserved by scripts/deploy-a30.sh.

PLUMOS_STANDALONE_USE_STOCK_SDL=0

PLUMOS_A30_OPENBOR_CPU_POLICY=fixed
PLUMOS_A30_OPENBOR_CPU_FREQ=1344000
PLUMOS_A30_OPENBOR_CPU_CORES=4

SDL_VIDEODRIVER=dummy

PLUMOS_A30_OPENBOR_DIRECT_PRESENT=1
PLUMOS_A30_OPENBOR_DIRECT_ROTATION=ccw
PLUMOS_A30_OPENBOR_DIRECT_LOGICAL_SIZE=640x480
PLUMOS_A30_OPENBOR_DIRECT_BGR565=1
PLUMOS_A30_OPENBOR_DIRECT_LINEAR=0
PLUMOS_A30_OPENBOR_RENDERER_OPENGL=0
PLUMOS_A30_OPENBOR_ROTATION=none
PLUMOS_A30_OPENBOR_SCALE=fit
PLUMOS_A30_OPENBOR_FULLSCREEN=0
PLUMOS_A30_OPENBOR_FORCE_FB_FRONT_PAGE=0

PLUMOS_A30_OPENBOR_FIX_CONTROLS=1
PLUMOS_A30_OPENBOR_FORCE_CONTROLS=0

PLUMOS_A30_OPENBOR_JOYSTICKD_TRIGGER_MODE=buttons
PLUMOS_A30_OPENBOR_JOYSTICKD_SHOULDER_LAYOUT=user
PLUMOS_A30_OPENBOR_JOYSTICKD_X_SOURCE=axisYR
PLUMOS_A30_OPENBOR_JOYSTICKD_Y_SOURCE=axisXR
PLUMOS_A30_OPENBOR_JOYSTICKD_FUNCTION_BUTTON=none
EOF
  append_manifest "config=plumos/config/standalone/openbor.env"

  if red_viper_build_enabled; then
    cat >"${config_dir}/red_viper.env" <<'EOF'
# Red Viper launcher overrides for Miyoo A30.
# This file is user-mutable and is preserved by scripts/deploy-a30.sh.

PLUMOS_A30_RED_VIPER_CPU_POLICY=fixed
PLUMOS_A30_RED_VIPER_CPU_FREQ=648000
PLUMOS_A30_RED_VIPER_CPU_CORES=2

PLUMOS_A30_RED_VIPER_RENDERER=sdlgl
PLUMOS_A30_RED_VIPER_FB=/dev/fb0
PLUMOS_A30_RED_VIPER_INPUT=/dev/input/event3
PLUMOS_A30_RED_VIPER_ROTATION=ccw
PLUMOS_A30_RED_VIPER_SCALE=fit
PLUMOS_A30_RED_VIPER_SDLGL_ROTATION=ccw
PLUMOS_A30_RED_VIPER_SDLGL_SCALE=fit
PLUMOS_A30_RED_VIPER_SDLGL_PHYSICAL=480x640
PLUMOS_A30_RED_VIPER_EYE=both
PLUMOS_A30_RED_VIPER_COLOR=red
PLUMOS_A30_RED_VIPER_WAIT_VSYNC=1
PLUMOS_A30_RED_VIPER_FRAME_SKIP=0
PLUMOS_A30_RED_VIPER_FAST_FORWARD=0
PLUMOS_A30_RED_VIPER_FF_TOGGLE=0
PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK=0
PLUMOS_A30_RED_VIPER_PERF_INFO=0
PLUMOS_A30_RED_VIPER_AUDIO=alsa
PLUMOS_A30_RED_VIPER_ALSA_DEVICE=default
PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US=160000
PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS=6
PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS=8
PLUMOS_A30_RED_VIPER_AUDIO_GAP=fade
EOF
    append_manifest "config=plumos/config/standalone/red_viper.env"
  else
    append_manifest "config=plumos/config/standalone/red_viper.env skipped experimental_disabled"
  fi
}

build_one() {
  local id=$1
  local repo=$2
  local ref=$3
  local func=$4
  local log="${LOG_DIR}/${id}.log"
  local src="${SRC_ROOT}/${id}"
  local commit

  if ! emulator_selected "${id}"; then
    msg "skip ${id}: filtered"
    append_manifest "${id}: skipped"
    SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    return 0
  fi

  append_manifest "${id}:"
  append_manifest "  repo=${repo}"
  append_manifest "  ref=${ref}"

  : >"${log}"
  if ! clone_repo "${id}" "${repo}" "${ref}" "${log}"; then
    msg "failed ${id}: clone"
    append_manifest "  status=failed"
    append_manifest "  phase=clone"
    append_manifest "  log=docs/build-logs/${id}.log"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  commit=$(git -C "${src}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
  append_manifest "  commit=${commit}"
  msg "building ${id}"
  if "${func}" "${src}" >>"${log}" 2>&1; then
    append_manifest "  status=built"
    append_manifest "  log=docs/build-logs/${id}.log"
    stage_license_files "${id}" "${src}"
    BUILT_COUNT=$((BUILT_COUNT + 1))
    msg "built ${id}"
  else
    append_manifest "  status=failed"
    append_manifest "  phase=build"
    append_manifest "  log=docs/build-logs/${id}.log"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    msg "failed ${id}; see ${log}"
  fi
}

finish_manifest() {
  {
    echo
    echo "summary:"
    echo "  built=${BUILT_COUNT}"
    echo "  failed=${FAILED_COUNT}"
    echo "  skipped=${SKIPPED_COUNT}"
  } >>"${MANIFEST}"
}

overlay_preferred_sdl_runtime() {
  local sdl_runtime="${DIST_ROOT}/plumos-sdl2-runtime/plumos/lib"

  if [ ! -f "${sdl_runtime}/libSDL2-2.0.so.0" ]; then
    append_manifest
    append_manifest "sdl_runtime_overlay=none"
    append_manifest "sdl_runtime_overlay_reason=dist/plumos-sdl2-runtime not found"
    return 0
  fi

  mkdir -p "${TARGET_DIR}/plumos/lib"
  rsync -a "${sdl_runtime}/" "${TARGET_DIR}/plumos/lib/"
  chmod 0755 "${TARGET_DIR}/plumos/lib/ld-linux-armhf.so.3" 2>/dev/null || true
  append_manifest
  append_manifest "sdl_runtime_overlay=dist/plumos-sdl2-runtime"
  append_manifest "sdl_runtime_overlay_libSDL2=$(basename "$(readlink -f "${sdl_runtime}/libSDL2-2.0.so.0")")"
  append_manifest "sdl_runtime_overlay_libSDL3=$(basename "$(readlink -f "${sdl_runtime}/libSDL3.so.0" 2>/dev/null || printf '%s\n' "${sdl_runtime}/libSDL3.so.0")")"
}

prepare_dist
mkdir -p "${SRC_ROOT}"
write_standalone_launcher
write_standalone_config_defaults

build_one ppsspp "${PPSSPP_REPO}" "${PPSSPP_REF}" build_ppsspp
build_one scummvm "${SCUMMVM_REPO}" "${SCUMMVM_REF}" build_scummvm
build_one easyrpg "${EASYRPG_REPO}" "${EASYRPG_REF}" build_easyrpg
build_one openbor "${OPENBOR_REPO}" "${OPENBOR_REF}" build_openbor
build_one dosbox-staging "${DOSBOX_REPO}" "${DOSBOX_REF}" build_dosbox
build_one pcsx_rearmed "${PCSX_REARMED_REPO}" "${PCSX_REARMED_REF}" build_pcsx_rearmed
if red_viper_build_enabled; then
  build_one red_viper "${RED_VIPER_REPO}" "${RED_VIPER_REF}" build_red_viper
else
  msg "skip red_viper: experimental disabled"
  append_manifest "red_viper: skipped"
  append_manifest "  reason=experimental_disabled"
  SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
fi

overlay_preferred_sdl_runtime
finish_manifest

msg "done: built=${BUILT_COUNT} failed=${FAILED_COUNT} skipped=${SKIPPED_COUNT}"
if [ "${FAILED_COUNT}" -gt 0 ] && [ "${FAIL_ON_STANDALONE_ERROR}" = "1" ]; then
  exit 1
fi
exit 0
