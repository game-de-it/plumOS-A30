# plumOS 音楽プレイヤー

`plumos-music-player` は、Apps メニューから起動する plumOS ネイティブの音楽プレイヤーです。
GMU は A30 上の表示と操作が不安定だったため、Mali/FreeType 描画、`plumos-joystickd`
入力、OSS `/dev/dsp` 音声出力を直接使う小さな専用アプリとして実装します。

## 配置

- ビルド: `./scripts/docker-build.sh music-player`
- 成果物: `dist/plumos-music-player`
- 起動スクリプト: `/mnt/SDCARD/plumos/bin/plumos-music-player-launch`
- 本体: `/mnt/SDCARD/plumos/apps/music-player/bin/plumos-music-player.bin`

`plumos-music-player-launch` は `plumos-joystickd --device-mode xbox` を起動し、
終了時に joystickd を停止します。

## 対応ファイル

起動時に以下を再帰スキャンします。

- `/mnt/SDCARD/Music`
- `/mnt/SDCARD/Roms/music`
- `/mnt/SDCARD/Roms/MUSIC`

対応拡張子は `.mp3`, `.flac`, `.wav` です。デコードは miniaudio の pinned
single-header を使い、音声出力は A30 で実績のある OSS `/dev/dsp` へ
44.1kHz / stereo / S16_LE で書き込みます。

`.mp3` については ID3v2 `APIC` フレーム内の埋め込み画像を読み取り、プレイヤー右側へ
アルバムアートとして表示します。埋め込み画像は JPEG と PNG に対応します。画像がない
曲、または画像をデコードできない曲では `NO ART` / `art decode failed` を表示します。

## 操作

- `A` / `START`: 選択中の曲を再生、再生中の曲なら一時停止/再開
- `B` / `Function`: 終了
- `UP` / `DOWN`: 曲選択
- `LEFT` / `RIGHT`: 5 秒戻し / 5 秒送り
- `X` / `Y`: 前の曲 / 次の曲
- `SELECT`: EQ プリセット切替
- `L` / `R`: 音量を 5% 下げる / 上げる

EQ プリセットは `Flat`, `Bass`, `Treble`, `Vocal`, `Soft` です。実装は軽量な
3-band 風のソフトウェア処理で、低域、中域、高域のゲインを再生ループ内で適用します。

## 注意

- ユーザーの音楽ファイルは配布物へ含めません。
- 現時点のスキャンは起動時のみです。再スキャンしたい場合は一度終了して起動し直します。
- `/dev/dsp` を他プロセスが掴んでいると再生できません。検証時は古い emulator/app を
  `scripts/stop-a30-display-processes.sh` で停止してから確認します。
- 検証用に `PLUMOS_MUSIC_AUTOPLAY=1` と `PLUMOS_MUSIC_EXIT_AFTER_MS=ミリ秒` を
  指定できます。通常の Apps 起動では使いません。
