FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
ENV PLUMOS_TARGET=arm-linux-gnueabihf
ENV PLUMOS_MARCH=armv7-a
ENV PLUMOS_MFPU=neon-vfpv4
ENV PLUMOS_MFLOAT_ABI=hard
ENV CC=arm-linux-gnueabihf-gcc
ENV CXX=arm-linux-gnueabihf-g++
ENV AR=arm-linux-gnueabihf-ar
ENV RANLIB=arm-linux-gnueabihf-ranlib
ENV STRIP=arm-linux-gnueabihf-strip
ENV PKG_CONFIG_ALLOW_CROSS=1

RUN dpkg --add-architecture armhf \
  && apt-get update \
  && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    bash \
    binutils-arm-linux-gnueabihf \
    bison \
    build-essential \
    ca-certificates \
    clang \
    cmake \
    crossbuild-essential-armhf \
    curl \
    file \
    flex \
    gettext \
    git \
    iproute2:armhf \
    libasound2-dev:armhf \
    libegl1-mesa-dev:armhf \
    libexpat1-dev:armhf \
    libflac-dev:armhf \
    libfmt-dev:armhf \
    libfreetype6-dev:armhf \
    libgles2-mesa-dev:armhf \
    libharfbuzz-dev:armhf \
    libicu-dev:armhf \
    libjpeg-dev:armhf \
    liblhasa-dev:armhf \
    libmad0-dev:armhf \
    libmpg123-dev:armhf \
    libogg-dev:armhf \
    libopusfile-dev:armhf \
    libpixman-1-dev:armhf \
    libpng-dev:armhf \
    libsamplerate0-dev:armhf \
    libsndfile1-dev:armhf \
    libsdl1.2-dev:armhf \
    libsdl2-dev:armhf \
    libspeexdsp-dev:armhf \
    libtool \
    libvorbis-dev:armhf \
    libxmp-dev:armhf \
    libxml2-dev:armhf \
    make \
    meson \
    ninja-build \
    openssh-sftp-server:armhf \
    patch \
    pkg-config \
    python3 \
    python3-pip \
    rsync \
    strace:armhf \
    tar \
    unzip \
    wget \
    xz-utils \
    zip \
    zlib1g-dev:armhf \
    zstd \
  && mkdir -p /opt/plumos/samba-armhf /tmp/plumos-samba-debs \
  && cd /tmp/plumos-samba-debs \
  && for pkg in \
    samba:armhf \
    samba-common-bin:armhf \
    samba-libs:armhf \
    libacl1:armhf \
    libattr1:armhf \
    libaudit1:armhf \
    libavahi-client3:armhf \
    libavahi-common3:armhf \
    libbsd0:armhf \
    libcap-ng0:armhf \
    libc6:armhf \
    libcups2:armhf \
    libgnutls30:armhf \
    libgmp10:armhf \
    libhogweed6:armhf \
    libidn2-0:armhf \
    libjansson4:armhf \
    libldap-2.5-0:armhf \
    libldb2:armhf \
    libnettle8:armhf \
    libp11-kit0:armhf \
    libpam0g:armhf \
    libpopt0:armhf \
    libsasl2-2:armhf \
    libsasl2-modules-db:armhf \
    libtalloc2:armhf \
    libtasn1-6:armhf \
    libtdb1:armhf \
    libtevent0:armhf \
    libunistring2:armhf \
    libwbclient0:armhf; do \
      apt-get download "$pkg"; \
    done \
  && for deb in ./*.deb; do dpkg-deb -x "$deb" /opt/plumos/samba-armhf; done \
  && rm -rf /tmp/plumos-samba-debs \
  && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /workspace /opt/plumos/sysroot /opt/plumos/src /opt/plumos/cache \
  && chmod 0777 /workspace /opt/plumos/cache

WORKDIR /workspace

CMD ["/bin/bash"]
