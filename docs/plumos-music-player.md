# plumOS Music Player

`plumos-music-player` is a native plumOS music player launched from the Apps
menu. GMU was unstable on the A30 display/input path, so plumOS ships a small
dedicated player using Mali/FreeType rendering, `plumos-joystickd` input, and
OSS `/dev/dsp` audio output.

Japanese counterpart: [plumos-music-player.ja.md](plumos-music-player.ja.md)

## Placement

- Build: `./scripts/docker-build.sh music-player`
- Artifact: `dist/plumos-music-player`
- Launcher: `/mnt/SDCARD/plumos/bin/plumos-music-player-launch`
- Binary: `/mnt/SDCARD/plumos/apps/music-player/bin/plumos-music-player.bin`

`plumos-music-player-launch` starts `plumos-joystickd --device-mode xbox` before
launch and stops joystickd on exit.

## Supported Files

At startup the player recursively scans:

- `/mnt/SDCARD/Music`
- `/mnt/SDCARD/Roms/music`
- `/mnt/SDCARD/Roms/MUSIC`

Lightweight formats with known-good paths, `.mp3`, `.flac`, and `.wav`, use the
pinned miniaudio single-header decoder first. Other formats fall back to the
FFmpeg/libav decode path. Audio is normalized to 44.1 kHz stereo float PCM and
then written to the A30-tested OSS `/dev/dsp` path as S16_LE.

Scanned extensions include common audio formats such as `.mp3`, `.flac`, `.wav`,
`.m4a`, `.m4b`, `.mp4`, `.aac`, `.ogg`, `.opus`, `.wma`, `.aiff`, `.au`, `.caf`,
`.ac3`, `.wv`, `.ape`, `.mpc`, `.tta`, `.mka`, `.webm`, `.3gp`, `.amr`, `.ra`,
`.dts`, `.oma`, `.adx`, `.dsf`, and `.dff`, plus tracker/chiptune formats such
as `.mod`, `.xm`, `.s3m`, `.it`, `.vgm`, `.vgz`, `.spc`, `.nsf`, `.gbs`, `.gym`,
`.hes`, `.sap`, `.ay`, and `.kss`. Actual playback depends on the bundled
libavcodec/libavformat support.

For `.mp3`, embedded album art is read from the ID3v2 `APIC` frame first. Other
containers use FFmpeg/libav attached pictures. JPEG and PNG artwork are
supported. Tracks with no usable artwork show `NO ART` or `art decode failed`.

## Controls

- `A` / `START`: play the selected track, or pause/resume the current track
- `B` / `Function`: exit
- `UP` / `DOWN`: select track
- `LEFT` / `RIGHT`: seek backward/forward by 5 seconds
- `X` / `Y`: previous/next track
- `SELECT`: switch EQ preset
- `L` / `R`: lower/raise volume by 5%

EQ presets are `Flat`, `Bass`, `Treble`, `Vocal`, and `Soft`. The EQ is a small
3-band-style software adjustment applied inside the playback loop.

## Notes

- User music files are not included in release packages.
- Scanning currently happens only at startup. Exit and restart the player to
  rescan.
- Playback cannot start if another process owns `/dev/dsp`.
- Test-only variables are `PLUMOS_MUSIC_AUTOPLAY=1` and
  `PLUMOS_MUSIC_EXIT_AFTER_MS=<milliseconds>`.
