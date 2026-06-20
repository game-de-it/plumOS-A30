#!/usr/bin/env bash
set -euo pipefail

BUSYBOX_VERSION="${BUSYBOX_VERSION:-1.38.0}"
BUSYBOX_SHA256="${BUSYBOX_SHA256:-34f9ea6ff8636f2c9241153b9114eefa9e65674a45318ae1ef95bb5f31c53bb2}"
BUSYBOX_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
DOWNLOAD_DIR="${ROOT_DIR}/build/downloads"
BUILD_DIR="${ROOT_DIR}/build/busybox-${BUSYBOX_VERSION}"
SRC_DIR="${BUILD_DIR}/src"
PATCH_DIR="${ROOT_DIR}/docker/plumos-toolchain/patches"
DIST_DIR="${ROOT_DIR}/dist/plumos-userland"
BIN_DIR="${DIST_DIR}/plumos/bin"
GNU_BIN_DIR="${DIST_DIR}/plumos/gnu/bin"
GNU_LIBEXEC_DIR="${DIST_DIR}/plumos/gnu/libexec"
LIB_DIR="${DIST_DIR}/plumos/lib"
DOC_DIR="${DIST_DIR}/plumos/share/doc/busybox"
GNU_DOC_DIR="${DIST_DIR}/plumos/share/doc/gnu-userland"
TARBALL="${DOWNLOAD_DIR}/busybox-${BUSYBOX_VERSION}.tar.bz2"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"

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

download_busybox() {
  mkdir -p "$DOWNLOAD_DIR"

  if [ -f "$TARBALL" ] && [ "$(sha256_of "$TARBALL")" = "$BUSYBOX_SHA256" ]; then
    log "Using cached BusyBox tarball"
    return
  fi

  log "Downloading BusyBox ${BUSYBOX_VERSION}"
  curl -fsSL "$BUSYBOX_URL" -o "$TARBALL"

  actual_sha="$(sha256_of "$TARBALL")"
  [ "$actual_sha" = "$BUSYBOX_SHA256" ] || die "BusyBox SHA-256 mismatch: ${actual_sha}"
}

set_config() {
  local symbol="$1"
  local value="$2"
  local key="CONFIG_${symbol}"

  case "$value" in
    y|n) ;;
    *) die "invalid config value for ${symbol}: ${value}" ;;
  esac

  if grep -q "^${key}=" "${SRC_DIR}/.config"; then
    sed -i "s/^${key}=.*/${key}=${value}/" "${SRC_DIR}/.config"
  elif grep -q "^# ${key} is not set" "${SRC_DIR}/.config"; then
    sed -i "s/^# ${key} is not set/${key}=${value}/" "${SRC_DIR}/.config"
  else
    printf '%s=%s\n' "$key" "$value" >> "${SRC_DIR}/.config"
  fi
}

prepare_source() {
  rm -rf "$BUILD_DIR"
  mkdir -p "$SRC_DIR"
  tar -C "$SRC_DIR" --strip-components=1 -xf "$TARBALL"
  patch -d "$SRC_DIR" -p1 < "${PATCH_DIR}/busybox-1.38.0-ftpd-utf8-feat.patch"

  (
    cd "$SRC_DIR"
    make defconfig >/dev/null

    set_config STATIC y
    set_config FEATURE_INSTALLER y
    set_config FEATURE_VERBOSE_USAGE y
    set_config SHOW_USAGE y
    set_config LONG_OPTS y
    set_config FEATURE_HUMAN_READABLE y
    set_config FEATURE_PREFER_APPLETS n
    set_config FEATURE_SH_STANDALONE n

    set_config PS y
    set_config FEATURE_PS_WIDE y
    set_config FEATURE_PS_LONG y
    set_config FEATURE_PS_TIME y
    set_config FEATURE_PS_ADDITIONAL_COLUMNS y

    set_config TOP y
    set_config FEATURE_TOP_CPU_USAGE_PERCENTAGE y
    set_config FEATURE_TOP_CPU_GLOBAL_PERCENTS y
    set_config FEATURE_TOP_SMP_CPU y
    set_config FEATURE_TOP_DECIMALS y
    set_config FEATURE_TOPMEM y

    set_config DF y
    set_config FEATURE_DF_FANCY y
    set_config FREE y
    set_config FUSER y
    set_config LSOF y
    set_config WATCH y
    set_config TIMEOUT y

    set +o pipefail
    yes "" | make oldconfig >/dev/null
    oldconfig_status="${PIPESTATUS[1]}"
    set -o pipefail
    [ "$oldconfig_status" -eq 0 ] || exit "$oldconfig_status"
  )
}

build_busybox() {
  log "Building BusyBox ${BUSYBOX_VERSION} (${JOBS} jobs)"
  (
    cd "$SRC_DIR"
    make -j"$JOBS" \
      CROSS_COMPILE="$CROSS_COMPILE" \
      CFLAGS_EXTRA="-march=${PLUMOS_MARCH:-armv7-a} -mfpu=${PLUMOS_MFPU:-neon-vfpv4} -mfloat-abi=${PLUMOS_MFLOAT_ABI:-hard}" \
      busybox \
      > "${BUILD_DIR}/make.log" 2>&1
  ) || {
    tail -160 "${BUILD_DIR}/make.log" >&2 || true
    die "BusyBox build failed"
  }

  "${CROSS_COMPILE}strip" "${SRC_DIR}/busybox" 2>/dev/null || true
}

write_wrapper() {
  local applet="$1"
  local path="${BIN_DIR}/${applet}"

  mkdir -p "$(dirname "$path")"
  {
    printf '%s\n' '#!/bin/sh'
    printf 'exec /mnt/SDCARD/plumos/bin/busybox %s "$@"\n' "$applet"
  } > "$path"
  chmod 0755 "$path"
}

find_runtime_lib() {
  local soname="$1"
  local dir

  for dir in \
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
  "${CROSS_COMPILE}readelf" -d "$elf" 2>/dev/null | \
    awk '/NEEDED/ { gsub(/[][]/, "", $5); print $5 }'
}

COPIED_RUNTIME_LIBS=""

copy_runtime_lib_tree() {
  local soname="$1"
  local lib

  case " ${COPIED_RUNTIME_LIBS} " in
    *" ${soname} "*) return 0 ;;
  esac

  lib="$(find_runtime_lib "$soname")" || die "runtime library not found: ${soname}"
  install -m 0755 "$lib" "${LIB_DIR}/${soname}"
  COPIED_RUNTIME_LIBS="${COPIED_RUNTIME_LIBS} ${soname}"

  while IFS= read -r child; do
    [ -n "$child" ] || continue
    copy_runtime_lib_tree "$child"
  done < <(runtime_needed "${LIB_DIR}/${soname}")
}

copy_runtime_deps() {
  local elf="$1"
  local soname

  while IFS= read -r soname; do
    [ -n "$soname" ] || continue
    copy_runtime_lib_tree "$soname"
  done < <(runtime_needed "$elf")
}

write_gnu_wrapper() {
  local name="$1"
  local binary="$2"
  local path="${GNU_BIN_DIR}/${name}"

  mkdir -p "$(dirname "$path")"
  {
    printf '%s\n' '#!/bin/sh'
    printf '%s\n' 'PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"'
    printf '%s\n' 'exec "${PLUMOS_ROOT}/lib/ld-linux-armhf.so.3" \'
    printf '%s\n' '  --library-path "${PLUMOS_ROOT}/lib:/usr/lib:/lib" \'
    printf '  "${PLUMOS_ROOT}/gnu/libexec/%s" "$@"\n' "$binary"
  } > "$path"
  chmod 0755 "$path"
}

install_gnu_tool() {
  local name="$1"
  shift
  local candidate
  local src=""
  local dst="${GNU_LIBEXEC_DIR}/${name}.bin"

  for candidate in "$@"; do
    if [ -e "$candidate" ]; then
      src="$(readlink -f "$candidate")"
      break
    fi
  done

  [ -n "$src" ] || die "${name}: armhf binary not found; rebuild the Docker image"
  file "$src" | grep -q 'ARM' || die "${name}: ${src} is not an ARM binary"

  install -m 0755 "$src" "$dst"
  copy_runtime_deps "$dst"
  write_gnu_wrapper "$name" "$(basename "$dst")"

  {
    echo "== ${name} =="
    echo "source: ${src}"
    file "$dst"
    echo
    "${CROSS_COMPILE}readelf" -d "$dst" | grep NEEDED || true
    echo
  } >> "${GNU_DOC_DIR}/manifest.txt"
}

assemble_gnu_tools() {
  log "Assembling GNU/iproute2 userland tools"
  mkdir -p "$GNU_BIN_DIR" "$GNU_LIBEXEC_DIR" "$LIB_DIR" "$GNU_DOC_DIR"
  : > "${GNU_DOC_DIR}/manifest.txt"

  install_gnu_tool ip /bin/ip /sbin/ip /usr/sbin/ip
  install_gnu_tool rsync /opt/plumos/rsync-armhf/usr/bin/rsync
  install_gnu_tool ss /bin/ss /usr/bin/ss /sbin/ss /usr/sbin/ss
  install_gnu_tool strace /usr/bin/strace /bin/strace

  {
    echo "plumOS GNU-style userland tools"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
    echo "Debian package versions:"
    dpkg-query -W -f='${binary:Package}\t${Version}\n' \
      iproute2:armhf strace:armhf 2>/dev/null || true
    dpkg-query -W -f='rsync:armhf\t${Version}\n' \
      rsync 2>/dev/null || true
  } > "${GNU_DOC_DIR}/versions.txt"

  {
    find "$GNU_BIN_DIR" "$GNU_LIBEXEC_DIR" -type f -print
    find "$LIB_DIR" -maxdepth 1 -type f -print
  } | sort | xargs sha256sum > "${GNU_DOC_DIR}/SHA256SUMS"

  find "$LIB_DIR" -maxdepth 1 -type f -printf '%f\n' | sort > "${GNU_DOC_DIR}/runtime-libs.txt"

  cat > "${GNU_DOC_DIR}/README.txt" <<'EOF'
GNU-style userland tools for plumOS A30

Installed wrappers:
  /mnt/SDCARD/plumos/gnu/bin/ip
  /mnt/SDCARD/plumos/gnu/bin/rsync
  /mnt/SDCARD/plumos/gnu/bin/ss
  /mnt/SDCARD/plumos/gnu/bin/strace

The wrappers run armhf Debian binaries from:
  /mnt/SDCARD/plumos/gnu/libexec

They explicitly use the plumOS dynamic loader and library path:
  /mnt/SDCARD/plumos/lib/ld-linux-armhf.so.3
  /mnt/SDCARD/plumos/lib
EOF
}

assemble_package() {
  log "Assembling plumOS userland package"
  rm -rf "$DIST_DIR"
  mkdir -p "$BIN_DIR" "$DOC_DIR"

  install -m 0755 "${SRC_DIR}/busybox" "${BIN_DIR}/busybox"

  if [ ! -s "${SRC_DIR}/busybox.links" ]; then
    (cd "$SRC_DIR" && make busybox.links >/dev/null)
  fi

  sort -u "${SRC_DIR}/busybox.links" | while IFS= read -r link; do
    [ -n "$link" ] || continue
    applet="$(basename "$link")"
    case "$applet" in
      busybox) continue ;;
    esac
    write_wrapper "$applet"
  done

  assemble_gnu_tools

  cat > "${BIN_DIR}/plumos-env" <<'EOF'
#!/bin/sh
export PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"
export PATH="${PLUMOS_ROOT}/gnu/bin:${PLUMOS_ROOT}/bin:${PATH}"
if [ "$#" -eq 0 ]; then
  exec "${PLUMOS_ROOT}/bin/sh"
fi
exec "$@"
EOF
  chmod 0755 "${BIN_DIR}/plumos-env"

  cat > "${DOC_DIR}/README.txt" <<EOF
BusyBox ${BUSYBOX_VERSION} for plumOS A30

Installed under:
  /mnt/SDCARD/plumos/bin

Use:
  /mnt/SDCARD/plumos/bin/plumos-env sh
  PATH=/mnt/SDCARD/plumos/bin:\$PATH ps --help

Source:
  ${BUSYBOX_URL}

Patches:
  busybox-1.38.0-ftpd-utf8-feat.patch

SHA-256:
  ${BUSYBOX_SHA256}
EOF

  sort -u "${SRC_DIR}/busybox.links" > "${DOC_DIR}/applets.txt"
  sha256sum "${BIN_DIR}/busybox" > "${DOC_DIR}/busybox.sha256"

  {
    echo "plumOS userland package"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "busybox_version: ${BUSYBOX_VERSION}"
    echo "busybox_source_sha256: ${BUSYBOX_SHA256}"
    echo "patch: busybox-1.38.0-ftpd-utf8-feat.patch"
    echo
    file "${BIN_DIR}/busybox"
    echo
    "${CROSS_COMPILE}readelf" -h "${BIN_DIR}/busybox"
  } > "${DOC_DIR}/manifest.txt"

  printf 'Built %s\n' "$DIST_DIR"
  printf 'BusyBox wrappers: %s\n' "$(find "$BIN_DIR" -type f | wc -l)"
  printf 'GNU wrappers: %s\n' "$(find "$GNU_BIN_DIR" -type f | wc -l)"
}

download_busybox
prepare_source
build_busybox
assemble_package
