# Frontend prototype

最初の plumOS frontend は、画面描画を始める前の互換性スキャナです。A30 上で stock
SD カード構成を読み、`Emu`, `RApp`, `App`, `Themes` の `config.json` を列挙します。

## build

```sh
./scripts/docker-build.sh frontend
```

生成物:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

## deploy

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

## manual run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

manual mode は `0` で終了します。wrapper から起動される通常 mode では、現時点では
stock MainUI へ fallback するために `75` で終了します。
通常 mode は stdout を抑制し、詳細は `/mnt/SDCARD/plumos/logs/plumos-frontend.log`
へ記録します。必要な場合は `PLUMOS_FRONTEND_STDOUT=1` で stdout へも出力します。

## 現在読む情報

- `/mnt/SDCARD/Emu/*/config.json`
- `/mnt/SDCARD/RApp/*/config.json`
- `/mnt/SDCARD/App/*/config.json`
- `/mnt/SDCARD/Themes/*/config.json`
- `/mnt/SDCARD/Roms/recentlist.json` の存在確認

現時点で読む field:

- `label`
- `name`
- `launch`
- `rompath`
- `extlist`

## 実機確認

2026-06-06 に A30 上で manual mode を実行し、以下を確認しました。

```text
summary emu=18 rapp=27 app=5 themes=42
```

## 次に足すもの

- ROM directory の実ファイル列挙
- `imgpath` と artwork lookup
- `launchlist` の読み取り
- `gamelist` XML の扱い
- `recentlist.json` の parse/update
- SDL/input を使った最小UI
