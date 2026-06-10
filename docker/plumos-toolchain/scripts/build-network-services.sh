#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PACKAGE_DIR="${ROOT_DIR}/package/network-services/plumos"
DIST_DIR="${ROOT_DIR}/dist/plumos-network-services"
PLUMOS_DIR="${DIST_DIR}/plumos"
BIN_DIR="${PLUMOS_DIR}/bin"
LIB_DIR="${PLUMOS_DIR}/lib"
SSH_LIBEXEC_DIR="${PLUMOS_DIR}/ssh/libexec"
SAMBA_SBIN_DIR="${PLUMOS_DIR}/samba/sbin"
DOC_DIR="${PLUMOS_DIR}/share/doc/network-services"
MANIFEST="${DOC_DIR}/manifest.txt"

CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
READELF="${READELF:-${CROSS_COMPILE}readelf}"
CC="${CC:-${CROSS_COMPILE}gcc}"
SAMBA_COMPAT="${LIB_DIR}/plumos-samba-compat.so"

COPIED_RUNTIME_LIBS=""

log() {
  printf '%s\n' "==> $*"
}

find_runtime_lib() {
  local soname="$1"
  local dir

  for dir in \
    /opt/plumos/samba-armhf/lib/arm-linux-gnueabihf \
    /opt/plumos/samba-armhf/usr/lib/arm-linux-gnueabihf \
    /opt/plumos/samba-armhf/usr/lib/arm-linux-gnueabihf/samba \
    /opt/plumos/samba-armhf/lib \
    /opt/plumos/samba-armhf/usr/lib \
    /lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf \
    /usr/lib/arm-linux-gnueabihf/samba \
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

find_packaged_lib() {
  local soname="$1"
  local dir

  for dir in "$LIB_DIR" "${LIB_DIR}/samba"; do
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

copy_runtime_lib_tree() {
  local soname="$1"
  local lib

  case " ${COPIED_RUNTIME_LIBS} " in
    *" ${soname} "*) return 0 ;;
  esac

  if lib="$(find_packaged_lib "$soname" 2>/dev/null)"; then
    :
  else
    lib="$(find_runtime_lib "$soname")" || {
      printf 'warning: runtime library not found: %s\n' "$soname" >&2
      return 0
    }
    install -m 0755 "$lib" "${LIB_DIR}/${soname}"
    lib="${LIB_DIR}/${soname}"
  fi

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
  printf 'warning: armhf dynamic loader not found\n' >&2
  return 0
}

find_first() {
  local candidate
  for candidate in "$@"; do
    if [ -e "$candidate" ]; then
      readlink -f "$candidate"
      return 0
    fi
  done
  return 1
}

write_loader_wrapper() {
  local path="$1"
  local binary="$2"
  local prelude="$3"

  {
    printf '%s\n' '#!/bin/sh'
    printf '%s\n' 'PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/SDCARD/plumos}"'
    printf '%s\n' 'PLUMOS_SDCARD_ROOT="${PLUMOS_SDCARD_ROOT:-/mnt/SDCARD}"'
    if [ -n "$prelude" ]; then
      printf '%s\n' "$prelude"
    fi
    printf '%s\n' 'PLUMOS_LIB="${PLUMOS_ROOT}/lib"'
    printf '%s\n' 'export LD_LIBRARY_PATH="${PLUMOS_LIB}:${PLUMOS_LIB}/samba:${PLUMOS_ROOT}/samba/lib:${LD_LIBRARY_PATH:-}"'
    printf '%s\n' 'exec "${PLUMOS_LIB}/ld-linux-armhf.so.3" \'
    printf '%s\n' '  --library-path "${LD_LIBRARY_PATH}:/usr/lib:/lib" \'
    printf '  "%s" "$@"\n' "$binary"
  } > "$path"
  chmod 0755 "$path"
}

assemble_base() {
  log "Assembling network services package"
  rm -rf "$DIST_DIR"
  mkdir -p "$BIN_DIR" "$LIB_DIR" "$SSH_LIBEXEC_DIR" "$SAMBA_SBIN_DIR" "$DOC_DIR"
  cp -a "${PACKAGE_DIR}/." "$PLUMOS_DIR/"
  chmod 0755 "${BIN_DIR}/plumos-network-services"
  : > "$MANIFEST"
  {
    echo "plumOS network services"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
  } >> "$MANIFEST"
  copy_loader
}

install_sftp_server() {
  local src

  src="$(find_first /usr/lib/openssh/sftp-server /usr/lib/ssh/sftp-server 2>/dev/null || true)"
  if [ -z "$src" ]; then
    {
      echo "sftp-server: not installed in toolchain image"
      echo
    } >> "$MANIFEST"
    return 0
  fi

  log "Installing OpenSSH sftp-server"
  install -m 0755 "$src" "${SSH_LIBEXEC_DIR}/sftp-server.bin"
  copy_runtime_deps "${SSH_LIBEXEC_DIR}/sftp-server.bin"
  write_loader_wrapper \
    "${SSH_LIBEXEC_DIR}/sftp-server" \
    '${PLUMOS_ROOT}/ssh/libexec/sftp-server.bin' \
    'cd "${PLUMOS_SDCARD_ROOT}" 2>/dev/null || true'
  {
    echo "sftp-server: ${src}"
    file "${SSH_LIBEXEC_DIR}/sftp-server.bin"
    echo
  } >> "$MANIFEST"
}

install_samba_daemon() {
  local name="$1"
  local src="$2"
  local dst_bin="${SAMBA_SBIN_DIR}/${name}.bin"
  local dst_wrapper="${SAMBA_SBIN_DIR}/${name}"
  local prelude=""

  if [ -f "$SAMBA_COMPAT" ]; then
    prelude='export LD_PRELOAD="${PLUMOS_ROOT}/lib/plumos-samba-compat.so${LD_PRELOAD:+:${LD_PRELOAD}}"'
  fi

  install -m 0755 "$src" "$dst_bin"
  copy_runtime_deps "$dst_bin"
  write_loader_wrapper "$dst_wrapper" "\${PLUMOS_ROOT}/samba/sbin/${name}.bin" "$prelude"
  {
    echo "${name}: ${src}"
    file "$dst_bin"
    echo
  } >> "$MANIFEST"
}

install_samba_compat() {
  local src="${ROOT_DIR}/package/network-services/src/plumos_samba_compat.c"

  if [ ! -f "$src" ]; then
    return 0
  fi

  log "Building Samba kernel compatibility shim"
  "$CC" -shared -fPIC -O2 -Wall -Wextra -o "$SAMBA_COMPAT" "$src" -ldl
  copy_runtime_deps "$SAMBA_COMPAT"
  {
    echo "samba-compat: ${src}"
    file "$SAMBA_COMPAT"
    echo
  } >> "$MANIFEST"
}

install_samba() {
  local smbd
  local nmbd
  local samba_lib

  smbd="$(find_first /opt/plumos/samba-armhf/usr/sbin/smbd /usr/sbin/smbd /usr/bin/smbd 2>/dev/null || true)"
  if [ -z "$smbd" ]; then
    {
      echo "samba: smbd not installed in toolchain image"
      echo
    } >> "$MANIFEST"
    return 0
  fi

  samba_lib="$(find_first /opt/plumos/samba-armhf/usr/lib/arm-linux-gnueabihf/samba /usr/lib/arm-linux-gnueabihf/samba /usr/lib/samba 2>/dev/null || true)"
  if [ -n "$samba_lib" ]; then
    log "Copying Samba private libraries/modules"
    mkdir -p "${LIB_DIR}/samba"
    cp -aL "${samba_lib}/." "${LIB_DIR}/samba/"
  fi

  install_samba_compat

  log "Installing Samba smbd"
  install_samba_daemon smbd "$smbd"

  nmbd="$(find_first /opt/plumos/samba-armhf/usr/sbin/nmbd /usr/sbin/nmbd /usr/bin/nmbd 2>/dev/null || true)"
  if [ -n "$nmbd" ]; then
    log "Installing Samba nmbd"
    install_samba_daemon nmbd "$nmbd"
  fi

  if [ -d "${LIB_DIR}/samba" ]; then
    while IFS= read -r elf; do
      copy_runtime_deps "$elf"
    done < <(find "${LIB_DIR}/samba" -type f -name '*.so*' | sort)
  fi
}

install_fat_tools() {
  local fsck_fat
  local dst_bin="${BIN_DIR}/fsck.fat.bin"

  fsck_fat="$(find_first /usr/sbin/fsck.fat /sbin/fsck.fat 2>/dev/null || true)"
  if [ -z "$fsck_fat" ]; then
    printf 'error: fsck.fat not found; rebuild the Docker image with dosfstools:armhf\n' >&2
    return 1
  fi

  log "Installing FAT checker"
  install -m 0755 "$fsck_fat" "$dst_bin"
  copy_runtime_deps "$dst_bin"
  write_loader_wrapper "${BIN_DIR}/fsck.fat" '${PLUMOS_ROOT}/bin/fsck.fat.bin' ''
  cp "${BIN_DIR}/fsck.fat" "${BIN_DIR}/dosfsck"
  chmod 0755 "${BIN_DIR}/dosfsck"

  {
    echo "fsck.fat: ${fsck_fat}"
    file "$dst_bin"
    echo
  } >> "$MANIFEST"
}

write_docs() {
  cat > "${DOC_DIR}/README.txt" <<'EOF'
plumOS network services

Default share/home directory:
  /mnt/SDCARD

Services:
  FTP:
    BusyBox tcpsvd + ftpd, port 21, max 20 concurrent connections.
  SFTP:
    Dropbear SFTP subsystem. Requires a Dropbear build with SFTP enabled and
    /mnt/SDCARD/plumos/ssh/libexec/sftp-server. Uses existing SSH
    authentication on port 2222. The plumOS Dropbear build raises
    MAX_UNAUTH_PER_IP to 20 for parallel small-file transfers.
  Samba:
    Writable SMB share named SDCARD on port 445 for Windows/macOS network-drive
    mounting, max 20 connections. Use smb://A30_IP/SDCARD or
    \\\\A30_IP\\SDCARD with:
      username: plumos
      password: plumos
  USB Disk Mode:
    Hidden experimental helper, /mnt/SDCARD/plumos/bin/plumos-usb-disk-mode.
    It unmounts /mnt/SDCARD before exposing /dev/mmcblk0p1 as USB Mass Storage
    and is not meant for normal UI use until hardware validation is complete.
    The package includes fsck.fat/dosfsck for the remount recovery path.

Persistent service state:
  /mnt/SDCARD/plumos/config/network/services.conf

Runtime control:
  /mnt/SDCARD/plumos/bin/plumos-network-services status ftp
  /mnt/SDCARD/plumos/bin/plumos-network-services status ssh
  /mnt/SDCARD/plumos/bin/plumos-network-services start ftp
  /mnt/SDCARD/plumos/bin/plumos-network-services stop ftp
  /mnt/SDCARD/plumos/bin/plumos-network-services start-enabled
EOF

  {
    find "$PLUMOS_DIR" -type f -print
  } | sort | xargs sha256sum > "${DOC_DIR}/SHA256SUMS"
}

assemble_base
install_sftp_server
install_samba
install_fat_tools
write_docs

printf 'Built %s\n' "$DIST_DIR"
find "$DIST_DIR" -maxdepth 5 -type f | sed "s#${DIST_DIR}/##" | sort
