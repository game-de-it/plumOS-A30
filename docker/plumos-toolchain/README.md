# plumOS Toolchain Docker

plumOS 用の A30 build environment です。まずは Docker 内で armhf binary を作り、
生成物を `dist/` に出すところまでを最小ゴールにしています。

## image build

```sh
./scripts/docker-build.sh image
```

default image tag は `plumos-a30-toolchain:dev` です。変更する場合:

```sh
PLUMOS_DOCKER_IMAGE=plumos-a30-toolchain:local ./scripts/docker-build.sh image
```

## smoke build

```sh
./scripts/docker-build.sh smoke
```

生成物:

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

この smoke binary は `arm-linux-gnueabihf-gcc` で静的 link します。A30 の stock glibc
`2.23` に直接依存しない小さな実機確認用 binary として使います。

## shell

```sh
./scripts/docker-build.sh shell
```

repository root が container 内の `/workspace` として mount されます。

## A30 へ転送

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

実行例:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

## 現時点の制約

- この Dockerfile は最初の build/deploy loop を作るための土台です。
- RetroArch と libretro core の最終 build には、A30 向け sysroot と library 方針を
  追加していきます。
- 動的 link する binary は A30 の glibc `2.23` より新しい glibc へ依存しないよう、
  別途 sysroot または同梱 runtime を使う必要があります。
