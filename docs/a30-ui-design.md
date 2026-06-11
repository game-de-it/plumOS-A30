# A30 UI design rules

この文書は、A30 実機の 2.8-inch 画面で読める plumOS frontend UI を保つための
設計ルールです。対象は Mali renderer の TOP/ROM/Settings/SAFE/network 画面と、
今後追加する list/gallery 系 UI です。

## 画面前提

- A30 の物理画面は 2.8-inch。実機での可読性を PC capture の見た目より優先する。
- renderer は論理 `640x480` の横画面 UI を、A30 の `480x640` raw framebuffer へ
  回転した向きで描く。
- UI 確認では 90 度回転済み capture と、実機目視の両方を見る。

## 文字サイズ

- ユーザーに見えるすべての文字は `1x` 以上にする。`1x` 未満の文字は使わない。
- 現行 bitmap renderer の `1x` は、5x7 bitmap font を `scale=2` で描くサイズ:
  glyph `10x14px`、固定セル `12x14px`。
- `1.5x` は `scale=3`: 固定セル `18x21px`。
- `2x` は `scale=4`: 固定セル `24x28px`。A30 実機では主要テキストの基本サイズとして扱う。
- `1x` は、時刻、Wi-Fi/Battery、補助的な prompt など、ユーザーが強く意識しない文字の
  最小サイズとしてのみ使う。
- system 名、ROM 名、ROM 数、選択中の項目、実行/復旧/終了に関わる文言は原則 `2x`。
- 文字が収まらない場合は、短縮、折り返し、非表示、画面構成の変更で対応する。
  横方向だけ圧縮して `1x` 未満に見せることはしない。
- debug/status/path/cursor index など、ユーザーに不要な情報は production UI に出さない。
  出す必要がある場合も `1x` 以上にする。

## フォント

- Linux console 風 list UI の ASCII 文字は、固定セルの bitmap font を使う。
  column alignment は pixel 位置ではなく文字セル単位で設計する。
- console prompt で必要な lowercase、`@`、`#`、`~` などの ASCII 記号は bitmap font に
  持たせる。TTY/list UI の alignment 用 ASCII を proportional TTF に逃がさない。
- FreeType/TTF は、日本語など非 ASCII 文字、または grid alignment を必要としない画面で使う。
- TTF を column UI に使う場合は、等幅 font を選び、advance を固定セルへ丸める。
  proportional advance をそのまま column 計算に使わない。

## List UI

- 正式な表示モード名は `Text` と `Graphic` にする。従来 list/compact list と呼んでいた
  Linux console 風UIは `Text`、従来 gallery mode と呼んでいた画像中心UIは `Graphic` と呼ぶ。
- `TOP Mode` と `ROM Mode` はユーザーに別々に設定させない。ユーザー向け設定は
  `UI Mode: Text/Graphic` の1項目で、TOP/ROM/list/gallery 全体を切り替える。
- ユーザーに見せるラベルや設定値は、大文字・小文字を意識して表記する。
  例: `UI Settings`、`System Settings`、`UI Mode`、`Text`、`Graphic`。
- system 名と ROM 数は固定セル column で揃える。
- ROM 数 column は常に読める位置に置く。system 名が長い場合は system 名側を省略する。
- A30 の基準確認は `2x` で行う。`2x` で表示行数が減ることは許容し、可読性を優先する。
- 選択行の highlight は文字を隠さず、カーソル/番号/system名/ROM数が同時に読めること。
- prompt や command 風の装飾も `1x` 未満にしない。長い command は折り返すか短縮する。
- 点滅 cursor は `1x` の固定セル以上の大きさで描く。

## TTY list UI

現行の Linux console 風 TOP/list UI は次の仕様を基準にする。

- Mali renderer の通常 fallback 画面は TTY list UI だけにする。旧 mock 由来の
  classic list style は production UI では使わない。
- Header は `PLUMOS A30 TTY1`、時刻、Wi-Fi、Battery/Charge だけを表示する。
- `PATH`、`STATUS`、`ENTRIES`、`CURSOR`、debug/status line は表示しない。
- START 第一階層は `UI Settings`、`System Settings`、`Network Settings`、
  `Performance Settings`、`Apps`、`HELP`、`Shutdown` を基本にする。
- START、Apps サブメニュー、Settings の各サブ項目、HELP は左端アクセントバーをブルーにし、
  command prompt は表示しない。画面タイトルとリスト項目で現在位置を示す。
- TOP/ROM の SELECT core menu も Text/Graphic 共通の list screen とし、操作説明promptは
  表示しない。起動候補は `Cores < core_name >` として表示し、区切り線の下に
  `CPU freq < value >` と `CPU Cores < value >` を置く。上下で項目を選び、
  十字キー左右で選択中の `launch_profile` / CPU frequency / CPU core count を変更する。
- 決定/実行は必ず A、戻る/キャンセルは必ず B にする。十字キー左右はページ送りや
  設定値変更には使ってよいが、決定/実行/戻る/キャンセルには使わない。
- START/Apps/Settings/HELP の項目名は、`Performance Settings` などの正式名を切らずに
  表示するため、必要に応じて `1.5x` を使う。`1x` 未満にはしない。
- Settings 項目名と設定値は JSON/コード側の大文字・小文字を保って表示する。
  ただし TOP の system 名は Linux console 風の意匠として ASCII を大文字化する。
- Settings list では項目名を優先する。横幅に入る短い値だけ `項目: 値` として併記し、
  長い説明値や path はリスト上で無理に表示しない。詳細表示/編集UIで扱う。
- UI Settings に出す操作項目は、保存後に実際の controller UI 挙動へ反映されるものを
  優先する。theme 由来の状態など読み取り専用情報は checkbox や `< value >` 風に
  見せず、通常の情報行として表示する。
- Graphic theme 関連は `UI Settings` 直下に並べず、`Theme Settings` サブ項目へ
  まとめる。選択中 theme を変更する行の項目名は、右側の値と衝突しないよう
  `Graphic Theme` ではなく `Theme` と表示する。
- `Theme Settings` では、theme package 由来の `Name`、`Status`、`Layout`、`Font` は
  情報行として表示する。`TOP Layout`、`Transition`、`Time`、`Axis`、`Easing` は
  `/mnt/SDCARD/plumos/config/frontend/settings.json` にユーザー上書き値として保存し、
  theme JSON 自体は書き換えない。`Theme` を切り替えた時は個別上書きをクリアし、
  新しい theme の初期値から始める。
- ON/OFF、Enable/Disable、true/false 系は Settings list では checkbox として表示する。
  表記は `[x] 項目名` / `[ ] 項目名` を基準にする。Aでtoggle、右でON、左でOFF。
  `Scan On Enter` のように肯定形の項目名へ置き換えられる二択も checkbox として扱う。
- `Text` / `Graphic` などの選択系は `項目名 < Text >` のように表示し、十字キー左右で
  値を切り替える。右を押した直後は `>`、左を押した直後は `<` を1回だけ短く赤くする。
- 数値系も `項目名 < 500 >` のように表示し、十字キー左右で増減する。step幅は項目ごとに
  決める。
- Settings list の checkbox と `< value >` 操作部は、TOP の ROM 数 column と同じように
  画面右端へ寄せる。項目名は左、操作部は右という2カラムとして扱う。
- Settings 画面下部には2行の説明欄を置き、カーソル中項目の意味だけを `1x` 以上で
  表示する。操作は見た目から直感的にわかるため、フッターにボタン説明は出さない。
- Favorites と Recent は START 第一階層ではなく、TOP の system list と一緒に
  仮想 system として表示する方針にする。
- Prompt は `1x` bitmap font で表示する。`1x` 未満の横圧縮は使わない。
- TOP の prompt text は
  `root@PlumOS A30:~# ls -n -c ./systems/top` に固定する。
- ROM list の prompt text は白寄りにし、選択中ROMが属する実ディレクトリを
  full path で表示する。形式は
  `root@PlumOS A30:~# ls -n -c /mnt/SDCARD/Roms/<alias>`。
- Prompt が1行に収まらない場合は、固定セル幅で行末まで使ってから次行へ折り返す。
  早めの任意改行や横圧縮で収めない。
- Prompt cursor は `1x` 固定セル相当のボックス型にし、TTY mode の1秒周期再描画で点滅させる。
- TOP の prompt 文字と prompt cursor は白寄りにする。装飾としては見せるが、
  list項目より強くなりすぎない明度にする。
- System list の主要文字は `2x` を基準にする。
- TOP/ROM list の入力は、上下で1項目移動、右で1ページ送り、左で1ページ戻しを行う。

## Graphic Mode

`UI Mode` を `Graphic` にした場合の TOP/ROM/Favorites/Recent は、画像が未整備でも
成立するカード/プレビュー型UIを基準にする。Settings、HELP、SAFE、Network、System系の
操作画面は視認性と誤操作防止を優先し、当面は現行の青バー付きlist UIを使う。

- TOP は `PLUMOS A30 GUI` のカードUIにする。`tile_grid` は1ページ3列x2行の正方形6件、
  `tile_strip` は1ページ1行x2列の正方形2件を基準にし、system logo を判別できる大きさで表示する。
- TOP card は system logo、system 名、ROM数、fallback initials を表示する。
  画像がなくても空に見せない。
- Graphic TOP の十字キーはタイルの見た目に合わせる。`tile_grid` は左右で横移動、上下で縦移動とし、
  行端では前後の行へ回り込む。`tile_strip` は左右移動のみを基本にする。
  ページ境界を越える場合は前/次ページへ自然に移動する。
- Graphic TOP のページ遷移は theme の `graphic_mode.transition` で見た目だけを切り替える。
  現時点の正式値は `none` と `slide`。`slide` は次ページが下から上へ、前ページが上から下へ
  eased motion で入れ替わる上下方向の演出にする。theme は入力、ページサイズ、決定/戻る動作を変えない。
  transition の時間、軸、easing は theme の `transition_ms`、`transition_axis`、
  `transition_easing` で管理する。
- ROM list は左に選択リスト、右に選択中ROMのプレビューパネルを置く。1ページは8件を基準にする。
- ROM名は日本語を含めて正確に表示するため、GraphicのROM listではFreeType描画を優先する。
- ROM preview は既存ROM cacheの `media.thumbnail` を使う。PNG thumbnail が存在する場合は右側
  preview panel に contain 表示し、画像が無い/読めない場合は initials/fallback panel を表示する。
- Graphic mode の ROM list は既存 cache を先に表示し、scan refresh は background で行う。
  background refresh では `--with-thumbnails` を使い、FE 側の scan cache が ROM と
  thumbnail path の対応を `media.thumbnail` として作る。
- ROM preview panel のROM名/detail表示も ROM list と同じルールを継承する。ROM名は元表記を維持し、
  decomposed かな濁点/半濁点は表示時だけ合成する。長い文字列は選択中ROMと同じ marquee timer で
  `1000ms` 待ってから横スクロールし、行番号、debug/status、cursor代わりの縦バーは表示しない。
- Graphic mode の ROM list では X で Gallery list に入る。Gallery は背景、中央の選択ROM画像、
  左右端に見切れる前後ROM画像を表示し、左右で 1 件ずつ横 slide する。中央のROM画像枠は
  4:3を基準にし、下部のROM名は通常ROM listより大きく表示する。上下は 5 件 jump、A は起動、
  B/X は元の ROM list に戻る。
- Gallery のROM画像は背景パネルや影を描かず、画像だけを表示する。ROM名 marquee と左右slide は
  60fps 相当の再描画周期を維持する。
- Gallery slide 中は移動先のさらに隣のROM画像も先に端へ描き、slide完了時に端の画像が
  後から出現して見えないようにする。
- Gallery slide 中に追加の左右入力を受けた場合は、現在のslideを差し替えず次の移動先として
  queueし、slide完了後に次のslideへつなぐ。
- JPEG/WEBP は scanner のlookup対象だが、Graphic rendererの実描画はまずPNGを優先する。
- TOP/ROM/Favorites/Recent の左端アクセントバーは Text mode と同じオレンジを使う。
- 背景は黒に近いneutral、カードは暗いグレー/青緑、選択枠はオレンジを基準にする。
  紫・青一色、ベージュ一色などの単調なpaletteにはしない。
- Graphic modeでも文字は `1x` 未満にしない。主要項目は `2x`、カード内の大きいfallback initialsは
  装飾兼識別子として `4x` 程度を使ってよい。
- 上下の1項目移動はソフトウェア key repeat を持つ。物理 input event が repeat event を
  出さない場合でも、押しっぱなしで連続移動できるようにする。
- TOP では A で選択 system に入る。ROM list では A で選択 ROM を起動し、B で TOP へ戻る。
- TOP/ROM list のページは実際に表示できる行数を単位にする。TTY `2x` では8行を1ページとして扱う。
- 項目数が画面に収まらない場合、カーソルが属するページだけを描画し、上下移動や左右ページ送りで
  画面スクロールできるようにする。
- System 選択 cursor は `>` のみ。左アクセントバーや `|>` に見える装飾は使わない。
- System 名の前に行番号は表示しない。
- System 名は ASCII を大文字化して表示する。長い system 名は ROM 数 column を守るために
  system 名側を省略する。
- TOP の system 名、ROM 数、ROM list の通常 ROM 名は、初期デザインの白灰色に近い
  灰色寄りの色を基準にする。選択行は従来どおり黄色寄りでよい。
- ROM 名は正確性を優先し、大文字化しない。大文字・小文字・ASCII記号・日本語などの非ASCIIを
  できるだけ元の表記のまま表示する。
- ROM list の ROM 名は TTF/FreeType で描画する。同一 ROM list 内で bitmap font と
  TTF/FreeType を混在させない。選択中の ROM 名が右端に収まらない場合は、選択行だけ
  1000ms 待ってから横スクロールして全文を読めるようにする。未選択行は右端で切る。
- ROM list の system cursor `>` は bitmap font のまま使うが、ROM 名は TTF/FreeType のため、
  同じ y 座標では見た目の高さが揃わない。ROM list では cursor の y を光学的に補正し、
  TTF の ROM 名と中央が揃って見えるようにする。
- ROM list の選択背景も同じ理由で、bitmap行用の背景位置をそのまま使わない。
  TTF の ROM 名と cursor の見た目中心に合うよう、背景矩形の y と高さをROM list専用に補正する。
- decomposed UTF-8 のかな濁点/半濁点は、ROM 名の正本を変えず、表示時だけ可能な範囲で合成する。
- ROM 数は画面右寄せにする。`ROMS` の `S` が右端基準に揃うように、行ごとに右端から
  text width を引いて x 座標を決める。
- 選択行の highlight は横長背景だけにし、cursor、system 名、ROM 数を隠さない。
- 左端には細い縦アクセントバーを置く。TOP と ROM list はオレンジ、Settings はブルーを
  使う。バーは画面種別を視認しやすくするための装飾で、cursor や list column を兼ねない。
- 実機確認時は `/dev/fb0` owner が1 processだけであることを確認する。重複した
  `plumos-controller-ui-mali.bin` が残ると画面が混ざり、デザイン判断を誤る。

## 操作感と描画性能

- TOP と ROM list は、上下 key repeat 時の体感が大きく変わらないようにする。
  ROM list は TTF/FreeType を使うため TOP より重くなりやすいが、操作時に明らかな
  もたつきが出る状態は避ける。
- TOP の system list は固定セル bitmap font の軽量描画を維持する。固定幅 column と
  key repeat の滑らかさを壊すため、TOP の主要 system list を proportional TTF へ
  安易に置き換えない。
- ROM list は名前表示の正確性と行間の統一感を優先し、ROM 名全体を TTF/FreeType で
  描画する。ASCII だけ bitmap、非ASCIIだけ TTF のような混在表示には戻さない。
- ROM list の FreeType 描画では、毎フレーム・全行・全文字に対して重い処理を走らせない。
  文字幅は cache し、横スクロール判定に必要な幅計測は原則として選択行だけに限定する。
- FreeType glyph は 1 pixel ごとに矩形を出すのではなく、同じ row の連続 pixel を
  span としてまとめて描画する。これにより ROM list の key repeat 体感を TOP に近づける。
- 長い ROM 名の横スクロールは、カーソルが当たってから `1000ms` 待つ。スクロールは
  文字数単位ではなく時間ベースの pixel offset で行い、現行目安は約 `80px/sec`。
- カーソル移動、ページ移動、ROM list へ入る操作、Favorites/Recent へ入る操作では
  marquee timer を reset し、選択直後に文字が流れ始めないようにする。
- TTY mode の periodic redraw は、prompt cursor 点滅と ROM 名 marquee のために使う。
  操作による画面更新は入力処理直後に描画し、periodic redraw 待ちにしない。
- Up/Down の software key repeat は、物理 input repeat に依存せず FE 側で持つ。
  現行目安は初回 delay `350ms`、repeat interval `95ms`。この値を変更する場合は
  TOP と ROM list の両方を実機で比較する。
- 描画の滑らかさを評価するときは、必ず `/dev/fb0` owner が1 processだけであることを
  確認する。重複FEがあると実装の問題と誤認しやすい。

## Verification

- UI 変更後は `scripts/capture-a30-framebuffer.sh` で capture を残す。
- capture では、`1x` 未満の文字、重なり、column 崩れ、不要な debug/status 表示がないことを確認する。
- 実機確認前に、古い `plumos-controller-ui-mali.bin` が複数起動していないことを確認する。
  必要なら `scripts/stop-a30-display-processes.sh` で `/dev/fb0` 所有 process を止めてから起動する。
