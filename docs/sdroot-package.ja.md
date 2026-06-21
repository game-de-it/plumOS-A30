# plumOS SD root package

この文書は、ユーザー配布用の SD card root package を定義する。

## 目的

`dist/plumos-release-sdroot` または `dist/plumos-sdroot-package` は、A30 の `/mnt/SDCARD`
の配布用完成形である。

ユーザーは archive をフォーマット済み SD カードの root に展開するだけで、StockOS の boot flow から plumOS frontend を起動できる。
plumOS は stockOS の SD card runtime と分離して成立するものではなく、`miyoo/` などの stock SD payload に
plumOS runtime を overlay して配布する。

## 生成

SD root package は runtime package と stock SD payload を入力にして作る。

```sh
./scripts/build-runtime-package.py
./scripts/build-sdroot-package.py
```

既定の stock SD payload 入力は以下。

```text
artifacts/stock-sdl-probe/extracted/mnt/SDCARD
```

正式 release 前には、実機で正常動作していた stock SD card から ROM/BIOS/save/media/user-data を除いた
payload を再取得し、この入力を更新する。

script 生成物は以下に出力される。

- `dist/plumos-sdroot-package`
- `dist/plumos-sdroot-package.tar.gz`

現在のエンドユーザー向け release asset は、実機で正常動作していた SD card 由来の
`dist/plumos-release-sdroot` を監査し、外側 directory を持たない `.7z` として固める。

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
rm -f dist/plumos-sdroot-package.7z
cd dist/plumos-release-sdroot
COPYFILE_DISABLE=1 7zz a -t7z -mx=5 -mmt=on ../plumos-sdroot-package.7z .
cd ../..
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

`.7z` は SD card root へ一回解凍すればよい形式である。

archive は外側の package directory を持たず、展開先 root に直接以下の top-level entry を作る。

```text
miyoo/app/MainUI
miyoo/app/MainUI.stock
miyoo/app/keymon
miyoo/app/sdlloading
miyoo/lib/
Emu/                 # stock payload に存在する場合
RetroArch/           # stock payload に存在する場合
plumos/
LICENSE
THIRD_PARTY_NOTICES.md
THIRD_PARTY_NOTICES.ja.md
Bios/
Images/
Imgs/
Roms/
Saves/
README.txt
manifest.txt
sha256sum.txt
```

既存の実機 SD card から作った release staging directory をそのまま配布 archive にする場合は、
archive 化の前に release preflight を実行する。

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
```

`blocker` が出た場合は、明確な生成物/履歴/セーブ/バックアップだけを quarantine へ移してから
再確認する。

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot --clean
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
```

`warning` は自動削除しない。特に stock emulator subtree 内の BIOS/system ROM、`Imgs/` の legacy
artwork、`plumos/ssh/etc/authorized_keys` は配布方針として判断する。

## 含むもの

- stock SD payload
  - `miyoo/app/MainUI.stock`: stock MainUI fallback
  - `miyoo/app/keymon`, `miyoo/app/sdlloading`
  - `miyoo/lib/`: stock SDL1/SDL2 と関連 library
  - `Emu/`, `RetroArch/` など、stock payload に含まれる非ユーザーデータ runtime
- `miyoo/app/MainUI`: plumOS `MainUI.wrapper`。stock `MainUI` は `MainUI.stock` として残す
- `plumos/`: frontend、runtime、emulator、core、config default、helper
- `plumos/state/standalone/ppsspp/`: fresh SD card 用の PPSSPP factory state。
  既存 SD card 更新時は user-managed state として保護する
- `plumos/ssh/`: Dropbear SSH、起動/停止 script、`password.hash`、任意の `authorized_keys`
- `LICENSE`、`THIRD_PARTY_NOTICES.md`、`THIRD_PARTY_NOTICES.ja.md`
- `Bios/`, `Images/`, `Imgs/`, `Roms/`, `Saves/`: 空の placeholder
  - plumOS の通常サムネイル置き場は `Images/<system_id>/`
  - `Imgs/` は既存 StockOS artwork 互換/旧置き場として残す
- `README.txt`
- `manifest.txt`
- `sha256sum.txt`

## 含めないもの

- ROM
- BIOS
- save/state
- screenshot/video
- top-level `.config/` user/runtime state
- network secret
- user log
- build cache

## 注意点

この package は fresh/formatted SD card へ展開することを主目的にする。既存 StockOS SD card に上書き展開する場合は、事前に `miyoo/app/MainUI` を退避する。

既存 SD card へ安全に上書き更新したい場合は、`docs/runtime-package.md` の runtime installer を使う。

## 検証観点

正式 release 前に以下を確認する。

- `miyoo/app/MainUI` が executable な wrapper である。
- `plumos/bootstrap/MainUI.wrapper` と `miyoo/app/MainUI` が同一内容である。
- `miyoo/app/MainUI.stock`、`miyoo/app/keymon`、`miyoo/app/sdlloading` が存在する。
- `miyoo/lib/libSDL2-2.0.so.0` が存在する。
- `plumos/bin/plumos-controller-ui-mali` が存在する。
- `plumos/config/frontend/systems.json` が存在する。
- `plumos/ssh/start-ssh.sh` と `plumos/ssh/bin/dropbear` が存在する。
- `plumos/ssh/etc/password.hash` が存在し、初期パスワード `plumos` でログインできる。
- `plumos/ssh/etc/authorized_keys` は任意。入っていれば鍵認証も使える。
- `LICENSE`、`THIRD_PARTY_NOTICES.md`、`THIRD_PARTY_NOTICES.ja.md` が存在する。
- `Roms/`、`Bios/`、`Images/`、`Imgs/` などには placeholder 以外のユーザーデータが含まれない。
- `.7z` archive を空 directory に展開すると、その directory が SD card root と同じ構成になる。
- fresh SD card 相当の実機で boot する。
