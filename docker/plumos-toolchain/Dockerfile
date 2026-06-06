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
    build-essential \
    ca-certificates \
    clang \
    cmake \
    crossbuild-essential-armhf \
    curl \
    file \
    gettext \
    git \
    libsdl2-dev:armhf \
    libtool \
    make \
    meson \
    ninja-build \
    patch \
    pkg-config \
    python3 \
    python3-pip \
    rsync \
    tar \
    unzip \
    wget \
    xz-utils \
    zstd \
  && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /workspace /opt/plumos/sysroot /opt/plumos/src /opt/plumos/cache \
  && chmod 0777 /workspace /opt/plumos/cache

WORKDIR /workspace

CMD ["/bin/bash"]
