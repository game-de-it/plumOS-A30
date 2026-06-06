# A30 runtime probe

`plumos-runtime-probe` は、A30 の frontend/runtime 置き換え前に、最小限の
video/input/audio interface を確認するための静的リンク診断 binary です。

stock SDL library にはリンクしません。SDL2 を plumOS 配下へ同梱するまでは、
まず Linux interface を直接確認します。

## build

```sh
./scripts/docker-build.sh runtime-probe
```

生成物:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

## deploy/run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

安全な読み取り寄りの確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --input-ms 100 --no-audio'
```

短時間の framebuffer draw/restore と audio 状態確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'
```

`--allow-busy-audio` は、stock MainUI が PCM device を掴んでいる状態を失敗ではなく
観測結果として扱うための option です。

## options

- `--fb PATH`: framebuffer path。既定値は `/dev/fb0`
- `--input PATH`: input event path。既定値は `gpio-keys-polled` の自動検出
- `--dsp PATH`: OSS audio path。既定値は `/dev/dsp`
- `--draw-ms MS`: 小さい framebuffer patch を描いて復元する時間
- `--input-ms MS`: input event を poll する時間
- `--audio-ms MS`: OSS へ短い test tone を書く時間
- `--allow-busy-audio`: audio device の `EBUSY` を成功扱いにする
- `--no-video`, `--no-input`, `--no-audio`: 個別 probe を省略

環境変数 `PLUMOS_FB`, `PLUMOS_INPUT_EVENT`, `PLUMOS_DSP` でも path を上書きできます。

## 2026-06-06 実機結果

read-only 寄りの確認:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32 line=1920
video draw=skipped
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio skipped
result=ok
```

draw/audio 状態確認:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,0 bpp=32 line=1920
video draw=ok pixels=64x64 ms=80
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio path=/dev/dsp open=busy allowed=yes errno=16 Device or resource busy
result=ok
```

`offset` は stock MainUI の framebuffer/pan 状態により `0,0` または `0,640` として
観測されることがあります。

追加確認では、stock `MainUI` が `/dev/snd/pcmC0D0p` を保持していました。そのため、
stock frontend を動かしたままでは `/dev/dsp` への test tone 書き込みはできません。

## 判断

- video: `/dev/fb0` は 480x640、32bpp、line length 1920 として取得でき、短時間の
  draw/restore も成功
- input: `gpio-keys-polled` は `/dev/input/event3` として open/poll 可能
- audio: OSS `/dev/dsp` は存在するが、stock MainUI が PCM を保持している間は busy
- SDL2: stock library は存在するが、plumOS runtime としては未採用。自前 SDL2 package
  を用意してから linked probe を作る

stock `keymon` と直接 input event の比較は [A30 input policy](a30-input-policy.md) に
分離しました。次は、plumOS 同梱 SDL2 の build または物理ボタン mapping の確定に進みます。
