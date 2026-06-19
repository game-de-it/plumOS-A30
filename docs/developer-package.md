# plumOS developer package

この文書は、plumOS A30 の Docker/toolchain を第三者または将来の開発環境で再現するための developer package を定義する。

## 目的

`dist/plumos-dev-package` は、runtime package を作るための source/toolchain snapshot である。A30 実機へ直接展開する payload ではない。

package には git 管理されている source、Dockerfile、build script、patch、recipe、docs を含める。`build/`、`dist/`、`artifacts/`、ROM、BIOS、実機 log、生成済み binary は含めない。

## 生成

release 用 package は clean working tree で生成する。

```sh
./scripts/build-dev-package.py
```

生成物は以下に出力される。

- `dist/plumos-dev-package`
- `dist/plumos-dev-package.tar.gz`

作業中の preview package が必要な場合だけ `--allow-dirty` を指定できる。この場合、manifest に `git_dirty=yes` と dirty status が記録される。release 用には使わない。

## package layout

```text
plumos-dev-package/
  README.md
  manifest.txt
  sha256sum.txt
  source/
```

`source/` は repository の source snapshot であり、通常の開発作業はここで行う。

```sh
cd plumos-dev-package/source
./scripts/docker-build.sh image
./scripts/docker-build.sh frontend
./scripts/docker-build.sh libretro-cores
```

runtime package を作る場合は、必要な `dist/` artifact を build した後に以下を実行する。

```sh
./scripts/build-runtime-package.py
```

runtime package の詳細は `docs/runtime-package.md` を参照する。

ユーザー配布用の SD root package を作る場合は、runtime package 生成後に以下を実行する。

```sh
./scripts/build-sdroot-package.py
```

SD root package の詳細は `docs/sdroot-package.md` を参照する。

GitHub Release 用の asset 一式を作る場合は、SD root package と developer package を生成した後に以下を実行する。

```sh
./scripts/build-release-bundle.py --version <version>
```

release artifact の分け方は `docs/release-artifacts.md` を参照する。

## provenance

`manifest.txt` には以下を記録する。

- package build time
- git commit
- git branch
- working tree dirty state
- source file count
- source byte size
- required source paths

`sha256sum.txt` は package 内の通常ファイルの SHA-256 一覧である。

## 注意点

developer package は Docker image tarball を含めない。Docker image は host 環境差が大きく、巨大になりやすいため、`source/docker/plumos-toolchain/Dockerfile` から再 build する。

配布時に「すぐ実機へ入れる package」が必要な場合は SD root package を使う。developer package は build 再現用であり、A30 の `/mnt/SDCARD/plumos` へ直接展開しない。
