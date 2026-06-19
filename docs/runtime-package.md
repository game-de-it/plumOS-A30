# plumOS runtime package

この文書は、A30 の SD カードへ展開する plumOS runtime package の作り方と展開方法を定義する。

## 目的

`dist/plumos-runtime-package` は、実機の `/mnt/SDCARD/plumos` 配下へ展開する runtime 一式である。FE、launcher、RetroArch、libretro core、PicoArch、standalone emulator、Pyxel runtime、SDL/Mali runtime、補助 userland を 1 つの配布単位にまとめる。

通常ユーザー向けの release asset は、この runtime package を入力にした `dist/plumos-sdroot-package.tar.gz` とする。runtime package は、既存 SD card へ安全に上書き更新する場合や、SD root package の中間成果物として使う。

ROM、BIOS、ユーザーの save/state、既存設定、log は package の責務に含めない。installer は既存の mutable path を退避してから payload を上書きし、退避した path を戻す。

## 入力 artifact

`scripts/build-runtime-package.py` は以下の `dist/` artifact を順番に overlay する。後の component が同じ path を持つ場合は後勝ちになる。

- `dist/plumos-userland` optional
- `dist/plumos-bootstrap`
- `dist/plumos-frontend`
- `dist/plumos-joystickd`
- `dist/plumos-network-services`
- `dist/plumos-retroarch-practical`
- `dist/plumos-sdl2-runtime`
- `dist/plumos-python-runtime`
- `dist/plumos-pyxel-a30`
- `dist/plumos-libretro-cores-onion-lock-all`
- `dist/plumos-picoarch`
- `dist/plumos-standalone-emulators-adopted`

standalone emulator は通常配布で採用するものだけを別 staging に作る。

```sh
PLUMOS_STANDALONE_FILTER=ppsspp,scummvm,easyrpg,pcsx_rearmed,openbor \
FAIL_ON_STANDALONE_ERROR=1 \
TARGET_DIR=/workspace/dist/plumos-standalone-emulators-adopted \
./scripts/docker-build.sh standalone-emulators
```

## 生成

```sh
./scripts/build-runtime-package.py
```

生成物は以下に出力される。

- `dist/plumos-runtime-package`
- `dist/plumos-runtime-package.tar.gz`

package 内には以下の検証用 metadata を含める。

- `plumos/share/doc/plumos-runtime-package/manifest.txt`
- `plumos/share/doc/plumos-runtime-package/emulator-runtime-manifest.tsv`
- `plumos/share/doc/plumos-runtime-package/overlay-overrides.txt`
- `sha256sum.txt`

`overlay-overrides.txt` は、component overlay 時に内容の異なる同一 path が後勝ちで上書きされた一覧である。代表例は `plumos-env` と SDL runtime で、最終 payload は frontend の launcher と `a30mali` backend を持つ SDL runtime が勝つ必要がある。

## 展開

archive を SD カード root に展開し、installer を実行する。

```sh
tar -xzf plumos-runtime-package.tar.gz
cd plumos-runtime-package
./install-plumos-runtime.sh /mnt/SDCARD
```

installer は既存 `/mnt/SDCARD/plumos` がある場合、既定で `/mnt/SDCARD/plumos-backups/` に tar backup を作る。backup を作らない場合は以下を指定する。

```sh
PLUMOS_RUNTIME_BACKUP=0 ./install-plumos-runtime.sh /mnt/SDCARD
```

## 保持される mutable path

installer は以下を保持する。

- `plumos/config/frontend/settings.json`
- `plumos/config/system/settings.json`
- `plumos/config/network/settings.json`
- `plumos/config/performance/settings.json`
- `plumos/config/standalone`
- `plumos/state`
- `plumos/logs`
- `plumos/retroarch/config/retroarch-minimal.cfg`
- `plumos/retroarch/config/retroarch-practical.cfg`
- `plumos/retroarch/saves`
- `plumos/retroarch/states`
- `plumos/retroarch/playlists`

## 検証観点

package 生成時に `docs/emulator-runtime-manifest.tsv` を読み、FE から実行可能な runtime profile に必要な launcher、core、standalone binary が payload に存在することを検証する。

fresh SD card 相当の install/rollback 検証は release 前に別途行う。

SD card root へそのまま展開できるユーザー配布形は `docs/sdroot-package.md` を参照する。
