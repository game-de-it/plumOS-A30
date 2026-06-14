#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_DOCKER_IMAGE:-plumos-a30-toolchain:dev}"
PLATFORM="${PLUMOS_DOCKER_PLATFORM:-linux/amd64}"
DOCKERFILE="${ROOT_DIR}/docker/plumos-toolchain/Dockerfile"
DEFAULT_LIBRETRO_SERIAL_CORES="nestopia quicknes gambatte gpsp picodrive mednafen_pce_fast mednafen_supergrafx mednafen_ngp mednafen_lynx handy prosystem gw pokemini mednafen_vb dinothawr mrboom tgbdual"

usage() {
  cat <<EOF
Usage: $0 COMMAND [ARGS...]

Commands:
  image          Build the plumOS toolchain image.
  smoke          Build the armhf smoke binary into dist/docker-smoke.
  userland       Build the plumOS BusyBox userland into dist/plumos-userland.
  frontend       Build the frontend prototype into dist/plumos-frontend.
  joystickd      Build the A30 serial-to-uinput joystick daemon into dist/plumos-joystickd.
  network-services
                 Build FTP/SFTP/Samba service package into dist/plumos-network-services.
  python-runtime Build Python 3 runtime with pip support into dist/plumos-python-runtime.
  pyxel-a30      Build Pyxel A30 launcher and optional patched fallback into dist/plumos-pyxel-a30.
  runtime-probe  Build the A30 runtime probe into dist/plumos-runtime-probe.
  mali-egl-probe Build the A30 fbdev + Mali EGL probe into dist/plumos-mali-egl-probe.
  sdl2-runtime   Build upstream SDL3+sdl2-compat runtime into dist/plumos-sdl2-runtime.
  sdl2-probe     Build the SDL2 joystick/GameController probe into dist/plumos-sdl2-probe.
  retroarch-minimal
                 Build a minimal RetroArch display probe into dist/plumos-retroarch-minimal.
  retroarch-practical
                 Build the practical RetroArch runtime into dist/plumos-retroarch-practical.
  libretro-cores Build Class A/B libretro cores into dist/plumos-libretro-cores.
  standalone-emulators
                 Build standalone PPSSPP/ScummVM/EasyRPG/DOSBox/PCSX-ReARMed/Red Viper into dist/plumos-standalone-emulators.
  shell          Open an interactive shell in the toolchain container.
  run CMD...     Run an arbitrary command in the toolchain container.

Environment:
  PLUMOS_DOCKER_IMAGE     Docker image tag. Default: ${IMAGE}
  PLUMOS_DOCKER_PLATFORM  Docker platform. Default: ${PLATFORM}
  CORE_RECIPES            libretro core recipe file inside the container.
                           Default: /workspace/docker/plumos-toolchain/libretro-core-recipes.tsv.
  PLUMOS_CORE_FILTER      libretro-cores filter: plumos, onion, all, class-a,
                           class-b, class-ab, class-o, or comma-separated core ids.
                           Default: plumos.
  FAIL_ON_CORE_ERROR      Set to 1 to make libretro-cores exit non-zero on any core build failure.
  LIBRETRO_OPTIMIZATION_PROFILE
                           libretro core optimization profile: speed, compat,
                           aggressive, size, debug, or custom. Default: speed.
  LIBRETRO_ENABLE_LTO     Set to 1 to add LTO to common libretro core flags.
  LIBRETRO_EXTRA_CFLAGS   Extra libretro C flags appended to the selected profile.
  LIBRETRO_EXTRA_CXXFLAGS Extra libretro C++ flags appended to the selected profile.
  LIBRETRO_EXTRA_LDFLAGS  Extra libretro linker flags appended to the selected profile.
  LIBRETRO_FORCE_MAKE_TOOLCHAIN
                           Set to 1 to pass CC/CXX/AR/RANLIB as make command-line
                           variables for libretro cores. Default: 1.
  LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS
                           Set to 1 to replace Makefile debug/release -O flags
                           with the selected libretro optimization profile.
                           Default: 1.
  LIBRETRO_MAKE_OPT_FLAGS  Override the -O flags used by that Makefile patch.
  LIBRETRO_MAKE_ARGS_OVERRIDE
                           Override recipe make_args for single-core option probes.
  LIBRETRO_CORE_BUILD_CONCURRENCY
                           Number of libretro cores to build concurrently.
                           Default: 4.
  HOST_JOBS                Host job budget used to derive JOBS when JOBS is unset.
                           Default: container CPU count.
  JOBS                    Per-core parallel build job count passed into make/cmake.
                           Default: HOST_JOBS divided by LIBRETRO_CORE_BUILD_CONCURRENCY.
  BUILD_JOB_FALLBACKS     libretro-cores retry job counts after a failed build attempt.
                           Default: 1.
  LIBRETRO_SERIAL_CORES   Space- or comma-separated core ids that should start
                           directly with JOBS=1. Default: known serial-only cores.
  PLUMOS_STANDALONE_FILTER
                           standalone-emulators filter: all or comma-separated emulator ids.
  FAIL_ON_STANDALONE_ERROR
                           Set to 1 to make standalone-emulators exit non-zero on any emulator build failure.
  TARGET_DIR               Override standalone output path inside the container.
  PPSSPP_PATCH_MODE        PPSSPP patch mode: a30, display-ui, or vanilla. Default: a30.
  PPSSPP_STAGE_ID          PPSSPP output emulator id. Default: ppsspp.
  PPSSPP_BINARY_NAME       PPSSPP staged binary name. Default: PPSSPPSDL.
  PYTHON_VERSION           Python source version for python-runtime. Default: 3.14.6.
  PYTHON_SHA256            Python source SHA-256 for python-runtime.
  PYXEL_VERSION            Pyxel version for pyxel-a30. Default: 2.9.6.
  PYXEL_REF                Pyxel git ref for pyxel-a30. Default: v\$PYXEL_VERSION.
EOF
}

need_docker() {
  command -v docker >/dev/null 2>&1 || {
    echo "error: docker is required" >&2
    exit 1
  }
}

build_image() {
  need_docker
  docker build \
    --platform "$PLATFORM" \
    -t "$IMAGE" \
    -f "$DOCKERFILE" \
    "$ROOT_DIR"
}

ensure_image() {
  need_docker
  if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    build_image
  fi
}

docker_run_base=(
  --rm
  --platform "$PLATFORM"
  --user "$(id -u):$(id -g)"
  -e HOME=/tmp
  -e PLUMOS_MARCH=armv7-a
  -e PLUMOS_MFPU=neon-vfpv4
  -e PLUMOS_MFLOAT_ABI=hard
  -e CORE_RECIPES="${CORE_RECIPES:-/workspace/docker/plumos-toolchain/libretro-core-recipes.tsv}"
  -e PLUMOS_CORE_FILTER="${PLUMOS_CORE_FILTER:-plumos}"
  -e FAIL_ON_CORE_ERROR="${FAIL_ON_CORE_ERROR:-0}"
  -e LIBRETRO_OPTIMIZATION_PROFILE="${LIBRETRO_OPTIMIZATION_PROFILE:-speed}"
  -e LIBRETRO_ENABLE_LTO="${LIBRETRO_ENABLE_LTO:-0}"
  -e LIBRETRO_EXTRA_CFLAGS="${LIBRETRO_EXTRA_CFLAGS:-}"
  -e LIBRETRO_EXTRA_CXXFLAGS="${LIBRETRO_EXTRA_CXXFLAGS:-}"
  -e LIBRETRO_EXTRA_LDFLAGS="${LIBRETRO_EXTRA_LDFLAGS:-}"
  -e LIBRETRO_FORCE_MAKE_TOOLCHAIN="${LIBRETRO_FORCE_MAKE_TOOLCHAIN:-1}"
  -e LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS="${LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS:-1}"
  -e LIBRETRO_MAKE_OPT_FLAGS="${LIBRETRO_MAKE_OPT_FLAGS:-}"
  -e LIBRETRO_MAKE_ARGS_OVERRIDE="${LIBRETRO_MAKE_ARGS_OVERRIDE:-}"
  -e LIBRETRO_CORE_BUILD_CONCURRENCY="${LIBRETRO_CORE_BUILD_CONCURRENCY:-4}"
  -e HOST_JOBS="${HOST_JOBS:-}"
  -e JOBS="${JOBS:-}"
  -e BUILD_JOB_FALLBACKS="${BUILD_JOB_FALLBACKS:-1}"
  -e LIBRETRO_SERIAL_CORES="${LIBRETRO_SERIAL_CORES:-${DEFAULT_LIBRETRO_SERIAL_CORES}}"
  -e PLUMOS_STANDALONE_FILTER="${PLUMOS_STANDALONE_FILTER:-all}"
  -e FAIL_ON_STANDALONE_ERROR="${FAIL_ON_STANDALONE_ERROR:-0}"
  -e TARGET_DIR="${TARGET_DIR:-}"
  -e PPSSPP_PATCH_MODE="${PPSSPP_PATCH_MODE:-a30}"
  -e PPSSPP_STAGE_ID="${PPSSPP_STAGE_ID:-ppsspp}"
  -e PPSSPP_BINARY_NAME="${PPSSPP_BINARY_NAME:-PPSSPPSDL}"
  -e PYTHON_VERSION="${PYTHON_VERSION:-3.14.6}"
  -e PYTHON_SHA256="${PYTHON_SHA256:-}"
  -e PYXEL_VERSION="${PYXEL_VERSION:-2.9.6}"
  -e PYXEL_REF="${PYXEL_REF:-}"
  -e RUST_TOOLCHAIN="${RUST_TOOLCHAIN:-nightly-2026-06-12}"
  -v "${ROOT_DIR}:/workspace"
  -w /workspace
  "$IMAGE"
)

cmd="${1:-}"
case "$cmd" in
  image)
    build_image
    ;;
  smoke)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-smoke.sh
    ;;
  userland)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-busybox.sh
    ;;
  frontend)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-frontend.sh
    ;;
  joystickd)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-joystickd.sh
    ;;
  network-services|net-services)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-network-services.sh
    ;;
  python-runtime|python|python3)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-python-runtime.sh
    ;;
  pyxel-a30|pyxel)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-pyxel-a30.sh
    ;;
  runtime-probe|probe)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-runtime-probe.sh
    ;;
  mali-egl-probe|mali-egl|egl-probe)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-mali-egl-probe.sh
    ;;
  sdl2-runtime|sdl2rt)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-sdl2-runtime.sh
    ;;
  sdl2-probe|sdl2)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-sdl2-probe.sh
    ;;
  retroarch-minimal|retroarch)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-retroarch-minimal.sh
    ;;
  retroarch-practical|retroarch-full)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-retroarch-practical.sh
    ;;
  libretro-cores|cores)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-libretro-cores.sh
    ;;
  standalone-emulators|standalone|emulators-standalone)
    ensure_image
    docker run "${docker_run_base[@]}" /workspace/docker/plumos-toolchain/scripts/build-standalone-emulators.sh
    ;;
  shell)
    ensure_image
    docker run -it "${docker_run_base[@]}" /bin/bash
    ;;
  run)
    shift
    if [ "$#" -eq 0 ]; then
      usage >&2
      exit 1
    fi
    ensure_image
    docker run "${docker_run_base[@]}" "$@"
    ;;
  -h|--help|help)
    usage
    ;;
  "")
    usage >&2
    exit 1
    ;;
  *)
    echo "error: unknown command: $cmd" >&2
    usage >&2
    exit 1
    ;;
esac
