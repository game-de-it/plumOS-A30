# plumOS release artifacts

この文書は、plumOS A30 の release artifact の分け方と GitHub Release へ載せる単位を定義する。

## release channel

plumOS は end-user release と developer release を分ける。

### End-user release

end-user release は、A30 の SD カード root へそのまま展開する SD root package である。

配布 asset:

- `plumos-a30-sdroot-<version>.tar.gz`

含むもの:

- stock SD payload
  - `miyoo/app/MainUI.stock`
  - `miyoo/app/keymon`, `miyoo/app/sdlloading`
  - `miyoo/lib/` stock SDL/runtime library
  - stock payload に含まれる非ユーザーデータ runtime
- `miyoo/app/MainUI` plumOS boot wrapper
- frontend
- launcher
- RetroArch runtime
- 採用済み libretro core
- PicoArch runtime
- 採用済み standalone emulator
- Pyxel runtime
- SDL/Mali runtime
- joystick/network/userland helper
- fresh SD card 用の `plumos/state/standalone/ppsspp` factory state
- Dropbear SSH kit
- empty `Roms/`, `Bios/`, `Images/`, `Imgs/`, `Saves/` placeholder
  - `Images/` は plumOS の通常サムネイル置き場
  - `Imgs/` は StockOS artwork 互換/旧置き場として残す
- SD root manifest/checksum

含めないもの:

- ROM
- BIOS
- user save/state
- top-level `.config/` user/runtime state
- network secret
- build cache
- 開発用 source tree

SSH は初期パスワード `plumos` でログインできる。hash は
`plumos/ssh/etc/password.hash` に置く。`plumos/ssh/etc/authorized_keys` は任意の補助認証として扱い、
公開配布 archive には個人の公開鍵を入れない。

既存 SD card へ安全に上書き更新する用途では、内部成果物として `dist/plumos-runtime-package.tar.gz`
も生成できる。これは `install-plumos-runtime.sh` で既存設定や save/state を保持する更新用 package であり、
GitHub Release の通常ユーザー向け primary asset にはしない。

### Developer release

developer release は、SD root package と runtime package を再 build するための source/toolchain package である。

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

- `dist/plumos-sdroot-package.tar.gz`
- `dist/plumos-dev-package.tar.gz`

出力例:

```text
dist/plumos-release-<version>/
  plumos-a30-sdroot-<version>.tar.gz
  plumos-a30-developer-<version>.tar.gz
  RELEASE_NOTES.md
  manifest.txt
  SHA256SUMS
```

release 用 bundle は clean working tree で生成する。preview だけが必要な場合は
`--allow-dirty` を指定できるが、正式 release には使わない。

## GitHub Release に載せるもの

GitHub Release の asset は以下に固定する。

- `plumos-a30-sdroot-<version>.tar.gz`
- `plumos-a30-developer-<version>.tar.gz`
- `SHA256SUMS`
- `manifest.txt`
- `RELEASE_NOTES.md`

release body は `RELEASE_NOTES.md` の内容を元にする。

## 生成順

1. clean working tree にする。
2. runtime package の入力 artifact を build する。SSH 接続確認用に公開鍵も入れたい場合だけ
   `A30_AUTHORIZED_KEYS="$HOME/.ssh/id_ed25519.pub" ./scripts/build-ssh-kit.sh` で
   `dist/plumos-a30-ssh-kit` を作る。通常の公開配布では公開鍵を入れず、パスワードログインを初期入口にする。
3. 正常動作していた stock SD card から ROM/BIOS/save/media/user-data を除いた stock payload を
   `artifacts/stock-sdl-probe/extracted/mnt/SDCARD`、または
   `./scripts/build-sdroot-package.py --stock-sdcard-dir <path>` で指定する path へ用意する。
4. `./scripts/build-runtime-package.py` を実行する。
5. `./scripts/build-sdroot-package.py` を実行する。
   実機 SD card 由来の `dist/plumos-release-sdroot` を release staging として使う場合は、
   archive 化の前に `./scripts/audit-release-sdroot.py dist/plumos-release-sdroot` を実行し、
   `blocker` が残っていないことを確認する。`--clean` は明確な生成物/履歴/セーブ/バックアップだけを
   `artifacts/release-sdroot-audit/.../quarantine/` へ移す。
6. `./scripts/build-dev-package.py` を実行する。
7. `./scripts/build-release-bundle.py --version <version>` を実行する。
8. `SHA256SUMS` を検証する。
9. fresh SD card 相当の環境で SD root package の boot を検証する。
10. 必要に応じて既存 SD card 向け runtime install/rollback を検証する。
11. GitHub Release へ release bundle 内の asset を upload する。

## 完了条件

正式 release は以下を満たす必要がある。

- runtime package が `docs/emulator-runtime-manifest.tsv` の runtime/core/binary 検証を通過している。
- SD root package が stock SD payload、`miyoo/app/MainUI`、`plumos/` runtime を含み、ROM/BIOS/user data を含まない。
- developer package の manifest が `git_dirty=no` である。
- release bundle の manifest が `git_dirty=no` である。
- `SHA256SUMS` が全 asset で検証できる。
- fresh SD card 相当の boot 検証が完了している。

fresh SD card 検証が未完了の間は、GitHub Release を draft または pre-release として扱う。
