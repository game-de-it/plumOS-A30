# Miyoo Mini Flip 移植メモ

この文書は、plumOS-A30 を Miyoo Mini Flip、以下 MMF、へ移植する時の技術的なリスク
マップです。A30 の runtime をそのままコピーできる、という意味ではありません。A30 で
得られた有効な知見、A30 固有なので取り除くべき前提、MMF 実機で最初に調べるべき項目を
整理します。

英語版: [porting-miyoo-mini-flip.md](porting-miyoo-mini-flip.md)

## 公開情報からの仮説

公開されている MMF 仕様は、あくまで初期仮説として扱います。実装やパッケージ判断の前に
必ず実機で確認してください。

公開情報では、MMF はおおむね以下のように説明されています。

- SigmaStar SSD202D / 1.2 GHz 前後の dual-core Cortex-A7。
- 128 MB DDR3 RAM。
- 2.8 inch IPS、`750x560` 前後の画面。
- Linux / OnionOS 系の SD カード software stack。
- 2.4 GHz Wi-Fi、mono speaker、analog stick なし、analog trigger あり。

参照しやすい外部情報:

- [Retro Handhelds first impressions](https://retrohandhelds.gg/miyoo-mini-flip-first-impressions/)
- [Retro Handhelds setup guide](https://retrohandhelds.gg/miyoo-mini-flip-setup-guide/)
- [Retro Catalog specifications](https://retrocatalog.com/retro-handhelds/miyoo-mini-flip)
- [SteamDeckHQ review/spec summary](https://steamdeckhq.com/news/miyoo-mini-flip-review/)

特に危険な仮説は画面 mode と input topology です。A30 は物理 `480x640` framebuffer に
対して `640x480` landscape 規約を作っています。一方、MMF の公開情報にある `750x560` は
A30 とはかなり違います。A30 の display 定数を継承せず、MMF 専用の device profile を
用意してください。

## 初回起動時の調査

launcher 変更や stock entry point 差し替えの前に、stock MMF SD カードの状態で hardware
と userland の inventory を保存します。

最低限、以下を取得します。

```sh
uname -a
cat /proc/cpuinfo
cat /proc/meminfo
cat /proc/cmdline
mount
cat /proc/bus/input/devices
ls -l /dev/fb* /dev/mali /dev/dri /dev/disp /dev/input /dev/snd 2>/dev/null
cat /sys/class/graphics/fb0/name 2>/dev/null
cat /sys/class/graphics/fb0/modes 2>/dev/null
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null
cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null
cat /proc/asound/cards 2>/dev/null
cat /proc/asound/devices 2>/dev/null
amixer scontrols 2>/dev/null
ps w
```

stock FE、stock RetroArch、stock standalone emulator の動作中には、以下も取得します。

```sh
for p in $(pidof MainUI retroarch PPSSPPSDL 2>/dev/null); do
  echo "== $p $(tr '\0' ' ' </proc/$p/cmdline) =="
  ls -l /proc/$p/fd 2>/dev/null
  cat /proc/$p/maps 2>/dev/null | grep -Ei 'SDL|EGL|GLES|Mali|asound|dsp'
done
```

stock FE、stock SDL、stock emulator、GPU/audio library には `readelf -A`、`readelf -d`、
`strings` を使います。早い段階で確認したいのは以下です。

- ABI: armv7 hard-float か、soft-float か、それ以外か。
- video path: stock SDL backend、direct framebuffer、fbdev EGL、DRM/KMS、vendor 独自経路のどれか。
- audio path: ALSA、OSS `/dev/dsp`、tinyalsa 風 helper、その他 mixer layer のどれか。

## 画面と HW 描画

A30 で一番重要だった知見は、汎用 Linux SDL backend だけでは足りなかったことです。A30 は
`/dev/fb0` と `/dev/mali` を持ちますが、DRM node はありません。plumOS で成立した経路は
clean-room の fbdev + Mali EGL です。

- `/dev/fb0` を開き、実 framebuffer geometry を読む。
- rootfs 側の vendor EGL/GLES stack を使う。
- fbdev EGL platform 経由で GLES2 context を作る。
- 必要に応じて offscreen/logical surface に描画する。
- 最終的に回転/拡大縮小済みの画像だけを物理 scanout へ present する。

A30 ではこれが SDL3 `a30mali` backend と、FE/PicoArch 用 presenter になりました。MMF では
`a30mali` を hardcode しないでください。新しい device profile を用意し、以下のどちらかに
します。

- profile-driven な汎用 `plumos-fbdev-egl` backend へ一般化する。
- MMF の EGL 初期化が A30 と十分に違う場合は、別の `mmfmali` backend を作る。

MMF の画面 bring-up はこの順番で進めます。

1. `/dev/mali`、`/usr/lib/libEGL.so`、`/usr/lib/libGLESv2.so` があるか確認する。
2. window system なしで EGL を初期化できるか確認する。
3. `EGL_VERSION`、GL vendor、GL renderer、surface size、swap 結果を記録する。
4. 既知の color grid を描画し、framebuffer dump と実機写真を両方取る。
5. 物理向き、logical landscape size、左右反転/上下反転/tiling の有無を決める。
6. その後に FE や emulator 経路へ進む。

A30 の landscape 規約を仮定しないでください。MMF が本当に `750x560` の場合、A30 とは別の
scaling policy が必要です。

- FE が使う logical screen。
- SDL2/GLES app に見せる logical screen。
- emulator 向けの整数スケール/アスペクト維持。
- thumbnail と screenshot の向き。
- power menu や overlay の座標系。

MMF の stock SDL video backend が動く場合も、それは調査入力として扱います。A30 でも一度は
stock SDL で表示できることを確認しましたが、最終的な Pyxel 対応は plumOS 管理の SDL runtime
へ寄せました。その方が、回転、スケール、cleanup、volume hotkey、将来の Pyxel update を
管理しやすかったためです。

## 入力と hotkey

A30 の input 実装はかなり device-specific です。A30 は物理ボタンが kernel input event に
出て、analog stick は `/dev/ttyS0` の serial protocol で来ます。`plumos-joystickd` はそれらを
`/dev/uinput` 経由の Xbox 風 virtual pad に変換しています。

MMF で同じ topology を仮定してはいけません。

公開情報では MMF は analog stick がなく、analog trigger があるとされています。つまり A30 の
serial stick reader は不要かもしれず、代わりに trigger の扱いが詰まりどころになる可能性が
あります。最初にやるべきことは全 input event の対応表作成です。

- すべての `/dev/input/event*` device を特定する。
- すべての物理ボタンと trigger を押し、`EV_KEY` と `EV_ABS` を記録する。
- lid、power、volume、function/menu event があるか確認する。
- stock helper daemon が `/dev/uinput` で virtual device を作っているか確認する。
- stock FE や daemon が input device を exclusive grab していないか確認する。

MMF には最低限、以下のような input profile を用意します。

```text
button_event = /dev/input/eventX
power_event = /dev/input/eventY
volume_event = /dev/input/eventZ
has_serial_stick = false
has_analog_triggers = true/false
uinput_gamepad = enabled only if emulators need a normalized pad
sdl_gamecontroller_mapping = generated from MMF, not copied from A30
```

stick がない hardware では、A30 joystick serial reader を起動しません。standalone app や
SDL2/RetroArch が evdev button を素直に読めるなら、virtual pad は trigger 名や function key
挙動を揃える時だけ必要になるかもしれません。

power/menu 挙動も MMF で別途確認します。A30 は safe-hotkey daemon で、FE 中でも emulator 中
でも power menu を表示します。MMF は clamshell なので lid switch が存在する可能性があります。
lid event がある場合は、sleep にするのか、power menu にするのか、sleep/resume が安定するまで
無視するのかを文書化してください。

## サウンド、mixer、音量調整

A30 の audio で重要だった知見は、隠れた mixer bypass を避けることです。standalone
PCSX-ReARMed が OSS `/dev/dsp` を使う build では音は出ましたが、ユーザーの音量調整が正しく
効きませんでした。standalone 経路を ALSA に寄せることで、実用上の音量問題を解決しました。

MMF の audio inventory では以下を確認します。

- `/dev/snd` 以下に何があるか。
- `/dev/dsp` も存在するか。
- stock FE が使う ALSA card と mixer control。
- speaker と headphone で control が違うか。
- mute/unmute や amplifier GPIO を stock helper が処理していないか。
- suspend/resume 後に PCM、mixer、amplifier state が失われないか。

plumOS から音量制御できる launch default を優先します。

```text
SDL_AUDIODRIVER=alsa
RetroArch audio_driver = "alsa"
standalone emulators: ALSA where available, OSS only after volume tests
plumos-volume-control: MMF-specific mixer control names
```

A30 の mixer 名をコピーしないでください。SoC 系統が近くても、codec、amplifier、headphone
detect、mixer label は board によって変わる可能性があります。

検証時は必ず以下を見ます。

- FE の効果音、または UI 無音ポリシー。
- 軽い core を使った RetroArch。
- standalone SDL2 app。
- PCSX-ReARMed のように OSS/ALSA の違いが問題になりやすい standalone emulator。
- ゲーム中の音量上下 hotkey。
- 音声再生中の sleep/resume。

## Boot、StockOS 境界、rollback

A30 では stock SD カードの `miyoo/app/MainUI` を wrapper に差し替え、`MainUI.stock` を残して
fallback できるようにしています。MMF では stock boot path、helper process、library layout が
違う可能性があります。

MMF で wrapper を入れる前に、以下を確認します。

1. stock FE entry point を boot script から特定する。
2. fallback に必要な stock SD payload を確認する。
3. Wi-Fi、key handling、audio amplifier、brightness が stock FE、shell script、daemon の
   どこで初期化されているか確認する。
4. rollback は SD カードだけで完結させる。
5. 初期 bring-up では NAND/rootfs を書き換えない。

MMF の release package は、MMF の stock SD payload を元に作る必要があります。A30 の
`miyoo/`、`lib/`、launcher script、fallback binary は MMF の復旧基盤にはなりません。

## 性能面の制約

公開情報が正しければ、MMF は dual-core Cortex-A7 と 128 MB RAM です。これは A30 というより
Miyoo Mini 系に近い性能です。影響は以下です。

- A30 の emulator list と default core を、そのまま MMF にコピーしない。
- scraper concurrency は、memory と CPU headroom を測るまで控えめにする。
- PicoArch と RetroArch の性能差は、presenter、audio pacing、core build flag、display path の
  組み合わせで A30 とは違う可能性がある。
- Pyxel や standalone GLES app は、EGL/SDL backend が安定してから試す。

Virtual Boy や Hatari で得た A30 の教訓は MMF でも有効です。「最新版」より core の version と
build 時期が効く場合があります。Onion/source-lock の調査結果は活用しつつ、MMF の ABI と性能
profile に合わせて source から build してください。

## Build system の分離ポイント

MMF を正式 target にする前に、A30-only な名前や前提を shared code から外します。

望ましい分離は以下です。

```text
device profile:
  miyoo-a30
  miyoo-mini-flip

profile owns:
  framebuffer geometry
  EGL/native-window setup
  SDL video driver name
  input event map
  SDL GameController mapping
  volume mixer controls
  CPU governor/core policy
  stock boot entry point
  release stock payload root
```

歴史的な文書や A30 専用 launcher に `A30` という名前が残るのは問題ありません。ただし新しい
共通 code の API では、本当に A30 固有の挙動でない限り `A30` を前提にしない方が安全です。

候補になる build target:

- `frontend`: profile で renderer 定数を選ぶ。
- `sdl2-runtime`: 汎用 fbdev EGL backend か、MMF 専用 backend。
- `joystickd`: MMF に stick がない場合は no-serial/button-only mode。
- `network-services`: busybox/dropbear 前提が同じなら再利用。
- `retroarch-practical` と `libretro-cores`: 確認済み MMF ABI で build。
- `picoarch`: presenter/input pacing を確認してから build。

## 最小 bring-up milestone

1. stock SD backup と hardware inventory。
2. FE crash 後も使える SSH または serial access。
3. simple color-grid probe による framebuffer dump と実機写真。
4. EGL/GLES probe、または HW 描画経路が使えないという明確な判断。
5. button、trigger、power、volume、必要なら lid event map。
6. ALSA mixer と volume-control の成立確認。
7. framebuffer owner が 1 process だけの状態で plumOS splash または minimal FE first frame。
8. MMF 物理ボタンで FE navigation。
9. RetroArch NES/GB の audio/exit 付き smoke test。
10. sleep/power menu test。
11. standalone emulator smoke test。
12. MMF stock SD root から release package を作り、fresh SD card で検証。

## 危険信号

以下が出た場合は、先へ進む前に止まって文書化します。

- `/dev/mali`、`/dev/dri`、EGL、GLES のどれも実用的に使えない。
- stock SDL は表示できるが、clean-room で再現しづらい private backend だけに依存している。
- framebuffer capture は正しいのに、物理 panel が回転、反転、tiling している。
- `/dev/uinput` がない、または使えない。
- stock key/power daemon が input を exclusive grab している。
- volume key は UI 状態を変えるが、実際の PCM/speaker volume が変わらない。
- OSS `/dev/dsp` 再生はできるが、plumOS volume control を無視する。
- sleep/resume 後に video は戻るが audio/input が戻らない。
- stock FE が初期化している hardware を plumOS がまだ初期化できていない。

危険信号が出た場合は、full FE や emulator stack を闇雲に patch するより、小さい probe と
結論の文書化を優先してください。

## 再利用すべき A30 文書

MMF 移植時に参照する価値が高い A30 文書は以下です。

- [A30 stock SDL video](../a30-stock-sdl-video.ja.md)
- [Pyxel A30 output](../pyxel-a30-output.ja.md)
- [A30 joystickd](../a30-joystickd.ja.md)
- [A30 input policy](../a30-input-policy.ja.md)
- [A30 system notes](../a30-system-notes.ja.md)
- [MainUI bootstrap](../mainui-bootstrap.ja.md)
- [Docker toolchain 詳細](../docker-toolchain.ja.md)
