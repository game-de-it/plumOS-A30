#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_DOCKER_IMAGE:-plumos-a30-toolchain:dev}"
PLATFORM="${PLUMOS_DOCKER_PLATFORM:-linux/amd64}"
DOCKERFILE="${ROOT_DIR}/docker/plumos-toolchain/Dockerfile"

usage() {
  cat <<EOF
Usage: $0 COMMAND [ARGS...]

Commands:
  image          Build the plumOS toolchain image.
  smoke          Build the armhf smoke binary into dist/docker-smoke.
  userland       Build the plumOS BusyBox userland into dist/plumos-userland.
  frontend       Build the frontend prototype into dist/plumos-frontend.
  joystickd      Build the A30 serial-to-uinput joystick daemon into dist/plumos-joystickd.
  runtime-probe  Build the A30 runtime probe into dist/plumos-runtime-probe.
  mali-egl-probe Build the A30 fbdev + Mali EGL probe into dist/plumos-mali-egl-probe.
  sdl2-runtime   Build upstream SDL3+sdl2-compat runtime into dist/plumos-sdl2-runtime.
  sdl2-probe     Build the SDL2 joystick/GameController probe into dist/plumos-sdl2-probe.
  shell          Open an interactive shell in the toolchain container.
  run CMD...     Run an arbitrary command in the toolchain container.

Environment:
  PLUMOS_DOCKER_IMAGE     Docker image tag. Default: ${IMAGE}
  PLUMOS_DOCKER_PLATFORM  Docker platform. Default: ${PLATFORM}
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
