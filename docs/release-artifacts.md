# plumOS release artifacts

この文書は、plumOS A30 の release artifact の分け方と GitHub Release へ載せる単位を定義する。

## release channel

plumOS は end-user release と developer release を分ける。

### End-user release

end-user release は、A30 の SD カードへ展開するための runtime package である。

配布 asset:

- `plumos-a30-runtime-<version>.tar.gz`

含むもの:

- frontend
- launcher
- RetroArch runtime
- 採用済み libretro core
- PicoArch runtime
- 採用済み standalone emulator
- Pyxel runtime
- SDL/Mali runtime
- joystick/network/userland helper
- install script
- runtime manifest/checksum

含めないもの:

- ROM
- BIOS
- user save/state
- network secret
- build cache
- 開発用 source tree

### Developer release

developer release は、runtime package を再 build するための source/toolchain package である。

配布 asset:

- `plumos-a30-developer-<version>.tar.gz`

含むもの:

- git 管理されている source snapshot
- Dockerfile
- build script
- patch
- libretro/standalone build recipe
- docs

含めないもの:

- Docker image tarball
- `build/`
- `dist/`
- `artifacts/`
- ROM/BIOS/user data

Docker image は host 環境差とサイズが大きいため、developer package から再 build する。

## release bundle

GitHub Release に載せる asset directory は以下で生成する。

```sh
./scripts/build-release-bundle.py --version <version>
```

既定では以下を入力にする。

- `dist/plumos-runtime-package.tar.gz`
- `dist/plumos-dev-package.tar.gz`

出力例:

```text
dist/plumos-release-<version>/
  plumos-a30-runtime-<version>.tar.gz
  plumos-a30-developer-<version>.tar.gz
  RELEASE_NOTES.md
  manifest.txt
  SHA256SUMS
```

release 用 bundle は clean working tree で生成する。preview だけが必要な場合は
`--allow-dirty` を指定できるが、正式 release には使わない。

## GitHub Release に載せるもの

GitHub Release の asset は以下に固定する。

- `plumos-a30-runtime-<version>.tar.gz`
- `plumos-a30-developer-<version>.tar.gz`
- `SHA256SUMS`
- `manifest.txt`
- `RELEASE_NOTES.md`

release body は `RELEASE_NOTES.md` の内容を元にする。

## 生成順

1. clean working tree にする。
2. runtime package の入力 artifact を build する。
3. `./scripts/build-runtime-package.py` を実行する。
4. `./scripts/build-dev-package.py` を実行する。
5. `./scripts/build-release-bundle.py --version <version>` を実行する。
6. `SHA256SUMS` を検証する。
7. fresh SD card 相当の環境で runtime install/rollback を検証する。
8. GitHub Release へ release bundle 内の asset を upload する。

## 完了条件

正式 release は以下を満たす必要がある。

- runtime package が `docs/emulator-runtime-manifest.tsv` の runtime/core/binary 検証を通過している。
- developer package の manifest が `git_dirty=no` である。
- release bundle の manifest が `git_dirty=no` である。
- `SHA256SUMS` が全 asset で検証できる。
- fresh SD card 相当の install/rollback 検証が完了している。

fresh SD card 検証が未完了の間は、GitHub Release を draft または pre-release として扱う。
