# Docker toolchain

plumOS は、基本的に Docker 内で build して、生成物を A30 へ転送して実機確認する流れで
開発します。

## 最初の確認

Docker image を作ります。

```sh
./scripts/docker-build.sh image
```

armhf の smoke binary を build します。

```sh
./scripts/docker-build.sh smoke
```

生成物は以下に出ます。

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

## A30 へ転送して実行

A30 の SSH が起動している状態で転送します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

実機上で実行します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

log を回収します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/collect-a30-logs.sh
```

## 方針

- Dockerfile、build script、patch、hash、recipe は git で管理する
- `build/`, `dist/`, `artifacts/` は生成物として git に入れない
- 動的 link が必要なものは、A30 向け sysroot または plumOS 同梱 runtime で扱う
- RetroArch と libretro core は、この build/deploy loop に載せて段階的に増やす

詳細は [plumOS 設計方針](plumos-design-policy.md) も参照してください。
