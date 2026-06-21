# Docker ビルドガイド

plumOS の A30 向け binary は、基本的に Docker 内でビルドします。host 側の compiler や
library に依存させず、Docker image、build script、patch、recipe を git 管理して再現性を保ちます。

## 前提

開発 PC に以下を用意します。

- Git
- Docker
- `7zz` または 7-Zip compatible tool
- A30 実機へ SSH 接続できる環境

標準の実機 target は以下です。

```text
root@192.168.10.165
port 2222
```

環境が違う場合は `A30_TARGET` と `A30_SSH_PORT` を指定します。

```sh
A30_TARGET=root@192.168.10.165 A30_SSH_PORT=2222 ./scripts/run-a30.sh 'uname -a'
```

## 1. Docker image を作る

最初に toolchain image を作ります。

```sh
./scripts/docker-build.sh image
```

以後の build command は、この image が無ければ自動で作成します。

## 2. 最小 smoke build

Docker と cross build の入口確認です。

```sh
./scripts/docker-build.sh smoke
```

生成物:

```text
dist/docker-smoke/
```

実機へ転送して実行します。

```sh
./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

## 3. 基本 runtime を build

frontend や emulator の前に、基礎 component を作ります。

```sh
./scripts/docker-build.sh userland
./scripts/build-bootstrap-package.sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh joystickd
./scripts/docker-build.sh network-services
./scripts/build-ssh-kit.sh
```

主な出力:

```text
dist/plumos-userland/
dist/plumos-bootstrap/
dist/plumos-frontend/
dist/plumos-joystickd/
dist/plumos-network-services/
dist/plumos-a30-ssh-kit/
```

## 4. 描画/runtime library を build

SDL/Mali 経路、Python/Pyxel、アプリ類を作ります。

```sh
./scripts/docker-build.sh sdl2-runtime
./scripts/docker-build.sh python-runtime
./scripts/docker-build.sh pyxel-a30
./scripts/docker-build.sh nextcommander
./scripts/docker-build.sh music-player
```

必要に応じて probe も作ります。

```sh
./scripts/docker-build.sh runtime-probe
./scripts/docker-build.sh mali-egl-probe
./scripts/docker-build.sh sdl2-probe
```

## 5. RetroArch と libretro core を build

通常利用する RetroArch runtime を作ります。

```sh
./scripts/docker-build.sh retroarch-practical
```

libretro core は recipe に基づいて build します。

```sh
./scripts/docker-build.sh libretro-cores
```

既定の `PLUMOS_CORE_FILTER=plumos` は plumOS default の採用対象を build します。
全候補を build したい場合は以下です。

```sh
PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 ./scripts/docker-build.sh libretro-cores
```

主な環境変数:

| 変数 | 用途 |
| --- | --- |
| `PLUMOS_CORE_FILTER` | `plumos`, `onion`, `all`, core id の絞り込み |
| `LIBRETRO_CORE_BUILD_CONCURRENCY` | 同時に build する core 数。既定は `4` |
| `JOBS` | 各 core 内の並列数 |
| `LIBRETRO_OPTIMIZATION_PROFILE` | `speed`, `compat`, `aggressive`, `size`, `debug` |
| `FAIL_ON_CORE_ERROR` | `1` で失敗 core があれば command 全体を失敗にする |

core の source/ref/build option は以下で管理します。

```text
docker/plumos-toolchain/libretro-core-recipes.tsv
docs/onion-libretro-source-lock.tsv
docs/libretro-core-version-inventory.tsv
```

## 6. PicoArch を build

PicoArch は lightweight libretro frontend です。core は原則として
`plumos/retroarch/cores/` の libretro core を共有します。

```sh
./scripts/docker-build.sh picoarch
```

生成物:

```text
dist/plumos-picoarch/
```

## 7. standalone emulator を build

採用済み standalone emulator を build します。

```sh
PLUMOS_STANDALONE_FILTER=ppsspp,scummvm,easyrpg,pcsx_rearmed,openbor \
FAIL_ON_STANDALONE_ERROR=1 \
TARGET_DIR=/workspace/dist/plumos-standalone-emulators-adopted \
./scripts/docker-build.sh standalone-emulators
```

個別に試す場合:

```sh
PLUMOS_STANDALONE_FILTER=ppsspp ./scripts/docker-build.sh standalone-emulators
```

## 8. runtime package を作る

各 component の `dist/` artifact を overlay して、`plumos/` runtime 一式を作ります。

```sh
./scripts/build-runtime-package.py
```

生成物:

```text
dist/plumos-runtime-package/
dist/plumos-runtime-package.tar.gz
```

`build-runtime-package.py` は `docs/emulator-runtime-manifest.tsv` を読み、FE から実行できる
profile に必要な binary/core が揃っているか検証します。

## 9. SD root package を作る

fresh SD card 向けの root 構成を作ります。

```sh
./scripts/build-sdroot-package.py
```

生成物:

```text
dist/plumos-sdroot-package/
dist/plumos-sdroot-package.tar.gz
```

現在の手動 release staging では、実機で正常動作している SD カードから ROM/BIOS/save/media を
除外して `dist/plumos-release-sdroot/` を作り、これを `.7z` 化しています。

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
rm -f dist/plumos-sdroot-package.7z
cd dist/plumos-release-sdroot
COPYFILE_DISABLE=1 7zz a -t7z -mx=5 -mmt=on ../plumos-sdroot-package.7z .
cd ../..
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

`.7z` は SD カード root へ一回解凍すればよい形式です。外側の package directory は含めません。

## 10. A30 へ deploy

通常 deploy は `scripts/deploy-a30.sh` を使います。

```sh
./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

`deploy-a30.sh` は既存の mutable config を保護します。設定を意図的に初期化したい場合だけ
`PLUMOS_DEPLOY_PROTECT_CONFIG=0` を指定します。

FE の起動/停止/再起動/状態確認は必ず `scripts/a30-fe-control.sh` を使います。

```sh
./scripts/a30-fe-control.sh restart
./scripts/a30-fe-control.sh status
```

`status` で `/dev/fb0` owner が 1 process だけであることを確認します。

## 11. 実機 command と log 回収

実機 command:

```sh
./scripts/run-a30.sh 'free -m'
./scripts/run-a30.sh 'ps | grep plumOS'
```

log 回収:

```sh
./scripts/collect-a30-logs.sh
```

## 12. よく使う検証

frontend:

```sh
./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
```

SDL2 gamepad:

```sh
./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

RetroArch minimal:

```sh
./scripts/docker-build.sh retroarch-minimal
./scripts/probe-a30-retroarch-minimal.sh --deploy --duration 10 --rotation ccw
```

Mali EGL:

```sh
./scripts/docker-build.sh mali-egl-probe
./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

## 13. release 前チェック

正式 release 前に最低限確認します。

```sh
git status --short
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

さらに fresh SD card へ展開して実機 boot を確認します。

## 詳細資料

- [Docker toolchain 詳細](../docker-toolchain.md)
- [runtime package](../runtime-package.md)
- [SD root package](../sdroot-package.md)
- [release artifacts](../release-artifacts.md)
- [emulator runtime manifest](../emulator-runtime-manifest.md)
