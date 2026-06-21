# 基本操作

plumOS frontend の基本操作です。ゲーム中の操作は emulator/runtime によって一部異なります。

## 共通ボタン

| ボタン | 動作 |
| --- | --- |
| D-pad 上下 | 項目移動 |
| D-pad 左右 | ページ移動、または設定値の変更 |
| A | 決定、実行 |
| B | 戻る、キャンセル |
| START | START menu を開く |
| POWER 短押し | Power menu を開く |
| Function | 主に emulator menu 用 |

TOP/ROM list は、上下で 1 項目移動、右で 1 ページ送り、左で 1 ページ戻しです。

## TOP 画面

TOP 画面には有効な system が表示されます。ROM がある system を選ぶと ROM list へ入ります。

Favorites と Recent は TOP の system list と同じ並びに表示されます。

## ROM list

ROM list では、選択した system の ROM を起動できます。

- A: 選択 ROM を起動
- B: TOP へ戻る
- START: START menu
- 左右: ページ移動

ROM list から TOP に戻り、再度同じ system に入ると、最後に選択していた ROM 位置へ戻ります。
Gallery mode でも同じ挙動です。

## Core Settings

system または ROM ごとに使用する runtime/core を選べます。

表示名の接頭辞は以下です。

| 接頭辞 | 意味 |
| --- | --- |
| RA | RetroArch |
| PICO | PicoArch |
| SA | Standalone |
| PYXEL | Pyxel |

ROM ごとの core 設定を `Default` に戻すと、system 側の core 設定を継承します。

## START menu

START menu の主な項目です。

| 項目 | 内容 |
| --- | --- |
| UI Settings | UI mode、表示、TOP 更新など |
| System Settings | 音量、明るさ、言語、情報、factory reset など |
| Network Settings | Wi-Fi 状態、NW Service |
| Performance Settings | emulator 起動時の CPU 周波数/コア数 |
| Apps | ファイルマネージャー、音楽プレイヤー、単体起動アプリ |
| HELP | ヘルプ |
| Shutdown | shutdown |

## UI Settings

`Refresh TOP` は TOP 画面の system/ROM 状態を再読み込みします。実行中は短い更新画面を表示します。
START menu を閉じるだけでは TOP の自動更新は行いません。

`起動時に前回ROMを開く` は以下の選択肢を持ちます。

| 値 | 動作 |
| --- | --- |
| Off | 起動時に ROM を開かない |
| On | 前回 ROM を直接開く |
| Recent | Recent 画面を開く |

## System Settings

主な項目です。

- Volume
- Brightness
- Lumination
- Display Color
- Language
- INFORMATION
- Factory Reset

Factory Reset は RetroArch、PicoArch、standalone emulator の設定を factory default へ戻すための機能です。

## Power menu

POWER 短押し、または shutdown 導線から Power menu を表示します。

| 項目 | 内容 |
| --- | --- |
| Sleep | sleep |
| Shutdown | shutdown |
| Cancel | 元の画面へ戻る |

ゲーム中でも POWER 短押しで Power menu を表示します。plumOS はゲーム内セーブや state save を
自動では行いません。sleep/shutdown 前のデータ保全はユーザーが行ってください。

## Network Settings

`NW Service` から以下を ON/OFF できます。

| サービス | 用途 |
| --- | --- |
| SSH | remote shell / SFTP の入口 |
| FTP | 高速なファイル転送 |
| SFTP | SSH 経由のファイル転送 |
| Samba | PC のネットワーク共有からアクセス |

共有 root は `/mnt/SDCARD/` です。

## Apps

Apps から起動できる主なアプリです。

- ファイルマネージャー
- 音楽プレイヤー
- RetroArch 単体起動
- PPSSPP 単体起動

## ゲーム中の menu

Runtime によって menu の出し方が異なります。

- RetroArch: Function button を menu 表示用に使う方針です。SELECT + button の hotkey は RetroArch 設定側で扱います。
- PicoArch: Function button で PicoArch menu を開きます。PicoArch menu では D-pad を使います。
- Standalone: emulator ごとに menu 操作が異なります。

Power menu は runtime に関係なく POWER 短押しで表示する方針です。
