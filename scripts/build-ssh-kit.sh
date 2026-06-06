#!/usr/bin/env bash
set -euo pipefail

DROPBEAR_VERSION="2026.91"
DROPBEAR_SHA256="defa924475abf6bc1e74abc00173e46bfdc804bd47caafa14f5a4ef0cc76da34"
DROPBEAR_URL="https://matt.ucc.asn.au/dropbear/releases/dropbear-${DROPBEAR_VERSION}.tar.bz2"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/dropbear-${DROPBEAR_VERSION}"
DOWNLOAD_DIR="${ROOT_DIR}/build/downloads"
SRC_DIR="${BUILD_DIR}/src"
DIST_DIR="${ROOT_DIR}/dist"
PKG_DIR="${DIST_DIR}/plumos-a30-ssh-kit"
ARCHIVE="${DIST_DIR}/plumos-a30-ssh-kit.tar.gz"
TARBALL="${DOWNLOAD_DIR}/dropbear-${DROPBEAR_VERSION}.tar.bz2"

JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

log() {
  printf '%s\n' "==> $*"
}

die() {
  printf '%s\n' "error: $*" >&2
  exit 1
}

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

download_dropbear() {
  mkdir -p "$DOWNLOAD_DIR"

  if [ -f "$TARBALL" ] && [ "$(sha256_of "$TARBALL")" = "$DROPBEAR_SHA256" ]; then
    log "Using cached Dropbear tarball"
    return
  fi

  log "Downloading Dropbear ${DROPBEAR_VERSION}"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$DROPBEAR_URL" -o "$TARBALL"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$TARBALL" "$DROPBEAR_URL"
  else
    die "curl or wget is required"
  fi

  actual_sha="$(sha256_of "$TARBALL")"
  [ "$actual_sha" = "$DROPBEAR_SHA256" ] || die "Dropbear SHA-256 mismatch: ${actual_sha}"
}

prepare_source() {
  rm -rf "$BUILD_DIR"
  mkdir -p "$SRC_DIR"
  tar -C "$SRC_DIR" --strip-components=1 -xf "$TARBALL"

  cat > "${SRC_DIR}/localoptions.h" <<'EOF'
#define DROPBEAR_SVR_PASSWORD_AUTH 0
#define DROPBEAR_SVR_PAM_AUTH 0
#define DROPBEAR_SVR_PUBKEY_AUTH 1
#define DROPBEAR_SFTPSERVER 0
#define DROPBEAR_X11FWD 0
#define DROPBEAR_SVR_AGENTFWD 0
#define DROPBEAR_SVR_LOCALTCPFWD 0
#define DROPBEAR_SVR_REMOTETCPFWD 0
#define DROPBEAR_SVR_LOCALSTREAMFWD 0
#define DROPBEAR_SVR_REMOTESTREAMFWD 0
EOF
}

configure_and_build() {
  zig_bin="$(command -v zig || true)"
  [ -n "$zig_bin" ] || die "zig is required for the musl static armhf build"

  log "Configuring Dropbear for arm-linux-musleabihf"
  (
    cd "$SRC_DIR"
    ./configure \
      --host=arm-linux-musleabihf \
      --enable-static \
      --disable-zlib \
      --disable-pam \
      --disable-shadow \
      --disable-lastlog \
      --disable-utmp \
      --disable-utmpx \
      --disable-wtmp \
      --disable-wtmpx \
      --disable-loginfunc \
      --enable-bundled-libtom \
      CC="${zig_bin} cc -target arm-linux-musleabihf -mcpu=cortex_a7" \
      AR="${zig_bin} ar" \
      RANLIB="${zig_bin} ranlib" \
      CFLAGS="-Os -Wno-undef" \
      LDFLAGS="-static" \
      > "${BUILD_DIR}/configure.log" 2>&1
  ) || {
    tail -80 "${BUILD_DIR}/configure.log" >&2 || true
    die "configure failed"
  }

  log "Building Dropbear (${JOBS} jobs)"
  (
    cd "$SRC_DIR"
    make -j"$JOBS" PROGRAMS="dropbear dropbearkey scp" > "${BUILD_DIR}/make.log" 2>&1
  ) || {
    tail -120 "${BUILD_DIR}/make.log" >&2 || true
    die "make failed"
  }
}

strip_binaries() {
  if command -v arm-linux-gnueabihf-strip >/dev/null 2>&1; then
    log "Stripping binaries with arm-linux-gnueabihf-strip"
    arm-linux-gnueabihf-strip "${SRC_DIR}/dropbear" "${SRC_DIR}/dropbearkey" "${SRC_DIR}/scp"
  elif command -v llvm-strip >/dev/null 2>&1; then
    log "Stripping binaries with llvm-strip"
    llvm-strip --strip-all "${SRC_DIR}/dropbear" "${SRC_DIR}/dropbearkey" "${SRC_DIR}/scp" || true
  elif [ "${PLUMOS_DOCKER_STRIP:-0}" = "1" ] && command -v docker >/dev/null 2>&1; then
    log "Stripping binaries with Docker/binutils-arm-linux-gnueabihf"
    docker run --rm -v "$SRC_DIR:/src" -w /src debian:bookworm-slim bash -lc \
      'apt-get update >/dev/null && apt-get install -y --no-install-recommends binutils-arm-linux-gnueabihf >/dev/null && arm-linux-gnueabihf-strip dropbear dropbearkey scp'
  else
    log "No ARM strip tool found; keeping unstripped binaries"
  fi
}

assemble_package() {
  log "Assembling SD card package"
  rm -rf "$PKG_DIR"
  mkdir -p "$PKG_DIR"
  cp -R "${ROOT_DIR}/package/ssh-kit/." "$PKG_DIR/"
  mkdir -p \
    "${PKG_DIR}/plumos/ssh/bin" \
    "${PKG_DIR}/plumos/ssh/etc" \
    "${PKG_DIR}/plumos/ssh/log" \
    "${PKG_DIR}/plumos/ssh/run" \
    "${PKG_DIR}/Roms/PORTS"

  install -m 0755 "${SRC_DIR}/dropbear" "${PKG_DIR}/plumos/ssh/bin/dropbear"
  install -m 0755 "${SRC_DIR}/dropbearkey" "${PKG_DIR}/plumos/ssh/bin/dropbearkey"
  install -m 0755 "${SRC_DIR}/scp" "${PKG_DIR}/plumos/ssh/bin/scp"

  if [ -n "${A30_AUTHORIZED_KEYS:-}" ]; then
    install -m 0644 "$A30_AUTHORIZED_KEYS" "${PKG_DIR}/plumos/ssh/etc/authorized_keys"
  elif [ ! -f "${PKG_DIR}/plumos/ssh/etc/authorized_keys" ]; then
    cp "${PKG_DIR}/plumos/ssh/etc/authorized_keys.example" "${PKG_DIR}/plumos/ssh/etc/authorized_keys"
  fi

  chmod 0755 \
    "${PKG_DIR}/plumos/ssh/start-ssh.sh" \
    "${PKG_DIR}/plumos/ssh/stop-ssh.sh" \
    "${PKG_DIR}/Roms/PORTS/Start SSH.sh" \
    "${PKG_DIR}/Roms/PORTS/Stop SSH.sh"

  rm -f "$ARCHIVE"
  tar -C "$DIST_DIR" -czf "$ARCHIVE" "$(basename "$PKG_DIR")"
}

print_summary() {
  log "Built ${ARCHIVE}"
  printf '\nPackage contents:\n'
  find "$PKG_DIR" -maxdepth 4 -type f | sed "s#${PKG_DIR}/##" | sort
  printf '\nBinaries:\n'
  ls -lh "${PKG_DIR}/plumos/ssh/bin/dropbear" \
    "${PKG_DIR}/plumos/ssh/bin/dropbearkey" \
    "${PKG_DIR}/plumos/ssh/bin/scp"
}

download_dropbear
prepare_source
configure_and_build
strip_binaries
assemble_package
print_summary
