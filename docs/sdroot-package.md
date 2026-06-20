# plumOS SD root package

この文書は、ユーザー配布用の SD card root package を定義する。

## 目的

`dist/plumos-sdroot-package` は、A30 の `/mnt/SDCARD` の配布用完成形である。

ユーザーは archive をフォーマット済み SD カードの root に展開するだけで、StockOS の boot flow から plumOS frontend を起動できる。

## 生成

SD root package は runtime package を入力にして作る。

```sh
./scripts/build-runtime-package.py
./scripts/build-sdroot-package.py
```

生成物は以下に出力される。

- `dist/plumos-sdroot-package`
- `dist/plumos-sdroot-package.tar.gz`

archive は外側の package directory を持たず、展開先 root に直接以下の top-level entry を作る。

```text
miyoo/app/MainUI
plumos/
Bios/
Images/
Imgs/
Roms/
Saves/
README.txt
manifest.txt
sha256sum.txt
```

## 含むもの

- `miyoo/app/MainUI`: plumOS `MainUI.wrapper`
- `plumos/`: frontend、runtime、emulator、core、config default、helper
- `plumos/ssh/`: Dropbear SSH、起動/停止 script、`authorized_keys`
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
- network secret
- user log
- build cache
- stock `MainUI.stock`

## 注意点

この package は fresh/formatted SD card へ展開することを主目的にする。既存 StockOS SD card に上書き展開する場合は、事前に `miyoo/app/MainUI` を退避する。

fresh SD card package には stock `MainUI.stock` を含めない。したがって、plumOS frontend 起動失敗時の stock fallback は既存 SD card へ wrapper install した場合より弱い。

既存 SD card へ安全に上書き更新したい場合は、`docs/runtime-package.md` の runtime installer を使う。

## 検証観点

正式 release 前に以下を確認する。

- `miyoo/app/MainUI` が executable な wrapper である。
- `plumos/bootstrap/MainUI.wrapper` と `miyoo/app/MainUI` が同一内容である。
- `plumos/bin/plumos-controller-ui-mali` が存在する。
- `plumos/config/frontend/systems.json` が存在する。
- `plumos/ssh/start-ssh.sh` と `plumos/ssh/bin/dropbear` が存在する。
- `plumos/ssh/etc/authorized_keys` に接続元 PC の公開鍵が入っている。
- `Roms/`、`Bios/`、`Images/`、`Imgs/` などには placeholder 以外のユーザーデータが含まれない。
- archive を空 directory に展開すると、その directory が SD card root と同じ構成になる。
- fresh SD card 相当の実機で boot する。
