# A30 UI Design Rules

This document defines the design rules for plumOS frontend UI on the A30's
2.8-inch physical screen. It applies to the Mali renderer TOP/ROM/Settings/SAFE
/ network screens and future list/gallery UI work.

## Display Assumptions

- The A30 physical screen is 2.8-inch. Real-device readability takes priority
  over how a PC capture appears.
- The renderer draws a logical `640x480` landscape UI into the A30 `480x640`
  raw framebuffer in the rotated orientation.
- UI review should use both a 90-degree-rotated capture and direct device
  inspection.

## Text Size

- All user-visible text must be at least `1x`. Do not use text smaller than
  `1x`.
- In the current bitmap renderer, `1x` means the 5x7 bitmap font rendered at
  `scale=2`: `10x14px` glyphs in a fixed `12x14px` cell.
- `1.5x` is `scale=3`: fixed `18x21px` cells.
- `2x` is `scale=4`: fixed `24x28px` cells. Treat this as the default size for
  primary text on the A30 device.
- `1x` is only the minimum size for low-priority text such as time,
  Wi-Fi/Battery, and secondary prompts.
- System names, ROM names, ROM counts, selected rows, and action/recovery/quit
  wording should normally be `2x`.
- When text does not fit, shorten it, wrap it, hide it, or change the layout.
  Do not horizontally compress text so it appears smaller than `1x`.
- Debug/status/path/cursor-index metadata should not appear in production UI.
  If it must appear, it still has to be at least `1x`.

## Fonts

- Linux-console-style list UI should use the fixed-cell bitmap font for ASCII.
  Column alignment should be designed in character cells, not arbitrary pixels.
- ASCII characters needed by console prompts, including lowercase, `@`, `#`,
  and `~`, should be available in the bitmap font. Do not fall back to a
  proportional TTF for alignment-sensitive ASCII in TTY/list UI.
- Use FreeType/TTF for non-ASCII text such as Japanese, or for screens that do
  not depend on grid alignment.
- If a TTF is used in a column UI, choose a monospaced font and quantize advance
  widths to the fixed grid. Do not let proportional advances drive column
  positions.

## List UI

- The official UI mode names are `Text` and `Graphic`. The Linux-console-style
  UI formerly called list/compact list is `Text`; the image-forward mode
  formerly called gallery mode is `Graphic`.
- Do not expose `TOP Mode` and `ROM Mode` as independent user settings. Use a
  single `UI Mode: Text/Graphic` setting to switch the overall TOP/ROM/list/
  gallery presentation.
- Preserve meaningful capitalization in user-facing labels and values.
  Examples: `UI Settings`, `System Settings`, `UI Mode`, `Text`, `Graphic`.
- Align system names and ROM counts with fixed character-cell columns.
- Keep the ROM count column readable. If a system name is too long, truncate the
  system name side.
- Use `2x` as the A30 baseline review size. It is acceptable for `2x` to show
  fewer rows; readability wins.
- Highlighted rows must not hide text. Cursor, number, system name, and ROM
  count should remain readable at the same time.
- Prompt or command-like decoration must also stay at least `1x`. Wrap or
  shorten long commands instead of rendering them below `1x`.
- A blinking cursor should be at least one `1x` fixed cell.

## TTY List UI

The current Linux-console-style TOP/list UI should follow these rules.

- The header shows only `PLUMOS A30 TTY1`, time, Wi-Fi, and Battery/Charge.
- Do not show `PATH`, `STATUS`, `ENTRIES`, `CURSOR`, or debug/status lines.
- The START first layer should be `UI Settings`, `System Settings`,
  `Network Settings`, `Performance Settings`, `Apps`, `HELP`, and `Shutdown`.
- START, Settings subpages, and HELP use the blue left accent bar and do not
  show a command prompt. The screen title and list entries indicate location.
- A is always confirm/run; B is always back/cancel. Left/Right may be used for
  page movement or setting value changes, but never for confirm/run/back/cancel.
- START/Settings/HELP entries may use `1.5x` when needed to show full formal
  labels such as `Performance Settings` without truncation. Never go below `1x`.
- Preserve capitalization for Settings entry names and values. TOP system names
  are the exception: ASCII system names stay uppercased as part of the
  Linux-console visual style.
- Settings lists prioritize entry names. Show only short values that fit as
  `Name: Value`; do not force long descriptions or paths into the list row.
  Handle those in later detail/edit UI.
- Prefer UI Settings controls whose saved values are reflected in real
  controller UI behavior. Read-only theme-derived state should be shown as
  normal information rows, not as checkbox or `< value >` controls.
- Keep Graphic theme rows under a `Theme Settings` subpage instead of listing
  them directly in `UI Settings`. Label the selected-theme choice `Theme`, not
  `Graphic Theme`, so the right-aligned value does not collide with the label.
- In `Theme Settings`, package-derived `Name`, `Status`, `Layout`, and `Font`
  stay as information rows. `TOP Layout`, `Transition`, `Time`, `Axis`, and
  `Easing` are saved as user overrides in
  `/mnt/SDCARD/plumos/config/frontend/settings.json`; do not rewrite the theme
  JSON package. Changing `Theme` clears those per-theme overrides so the newly
  selected package starts from its own defaults.
- ON/OFF, Enable/Disable, and true/false values appear as checkboxes in
  Settings lists. Use `[x] Name` / `[ ] Name`; A toggles, Right turns ON, and
  Left turns OFF.
  Binary choices that can be named positively, such as `Scan On Enter`, also
  appear as checkboxes.
- Choice values such as `Text` / `Graphic` appear as `Name < Text >` and change
  with Left/Right. After pressing Right, flash `>` red once briefly; after
  pressing Left, flash `<` red once briefly.
- Numeric values also appear as `Name < 500 >` and change with Left/Right. Each
  setting owns its own step size.
- In Settings lists, checkbox and `< value >` controls are right-aligned like
  the TOP ROM-count column. Treat labels and controls as two columns.
- Settings screens reserve a two-line footer for the selected item's meaning
  only, rendered at `1x` or larger. Do not show button hints there; the controls
  should be visually obvious.
- Favorites and Recent should live with the TOP system list as virtual systems,
  not in the START first layer.
- Render the prompt with the `1x` bitmap font. Do not use horizontal compression
  that makes it appear smaller than `1x`.
- TOP prompt text is fixed to
  `root@PlumOS A30:~# ls -n -c ./systems/top`.
- ROM-list prompt text is near-white and shows the real directory for the
  selected ROM as a full path. The format is
  `root@PlumOS A30:~# ls -n -c /mnt/SDCARD/Roms/<alias>`.
- If the prompt does not fit on one line, wrap at the fixed-cell line end and
  continue on the next line. Do not wrap early or compress it to fit.
- The prompt cursor is a box-shaped `1x` fixed cell and blinks through the TTY
  mode 1-second periodic redraw.
- TOP prompt text and the prompt cursor should be near-white. Keep it readable
  as decoration, but do not let it overpower the list entries.
- Main system-list text uses `2x` as the baseline.
- TOP/ROM list input uses Up/Down for one-item movement, Right for page down,
  and Left for page up.
- Up/Down one-item movement has software key repeat. Holding the physical key
  should move continuously even when the input device does not emit repeat
  events.
- On TOP, A enters the selected system. On ROM lists, A launches the selected
  ROM and B returns to TOP.
- TOP/ROM list pages are based on the number of rows that actually fit on
  screen. TTY `2x` uses 8 rows per page.
- When entries do not fit on screen, render only the page that contains the
  cursor and allow scrolling through Up/Down and Left/Right page movement.
- The system selection cursor is only `>`. Do not use a left accent bar or any
  decoration that reads as `|>`.
- Do not show row numbers before system names.
- Render ASCII system names uppercase. If a system name is too long, truncate
  the system-name side to protect the ROM-count column.
- TOP system names, ROM counts, and normal ROM-list names should use a
  grayish near-white color inspired by the early UI mockup. Selected rows may
  remain yellow.
- Preserve ROM name spelling. Do not uppercase ROM names; render uppercase,
  lowercase, ASCII symbols, and Japanese/non-ASCII text as faithfully as
  possible.
- ROM-list names use TTF/FreeType. Do not mix the bitmap font with TTF/FreeType
  within the same ROM list. When the selected ROM name does not fit at the right
  edge, wait 1000 ms and then horizontally scroll only the selected row so the
  full name can be read. Clip non-selected rows at the right edge.
- The ROM-list system cursor `>` remains bitmap, while ROM names use
  TTF/FreeType. Because the same y coordinate does not produce the same visual
  height, optically offset the cursor on ROM lists so it appears vertically
  centered with the TTF ROM name.
- For the same reason, ROM-list selection backgrounds must not reuse the bitmap
  row background position unchanged. Give ROM lists their own background y
  offset and height so the background visually centers around the TTF ROM name
  and cursor.
- For decomposed UTF-8 kana dakuten/handakuten, compose display glyphs where
  practical without changing the ROM-name source of truth.
- Right-align ROM counts. Compute each row's x position from the right edge so
  the `S` in `ROMS` aligns to the right-side baseline.
- Highlight selected rows with only a wide background, without hiding the
  cursor, system name, or ROM count.
- Add a thin vertical accent bar at the left edge. TOP and ROM lists use
  orange; Settings uses blue. This bar is a screen-type visual accent, not a
  cursor or list column.
- During device review, confirm there is only one `/dev/fb0` owner. Leftover
  `plumos-controller-ui-mali.bin` processes can mix multiple UI frames and make
  design review misleading.

## Graphic Mode

When `UI Mode` is `Graphic`, TOP/ROM/Favorites/Recent use a card/preview UI that
still works before artwork is curated. Settings, HELP, SAFE, Network, and System
operation screens keep the current blue-accent list UI for readability and
misclick prevention.

- TOP uses a `PLUMOS A30 GUI` card grid. One page shows a 3-column by 2-row
  grid of six square cards so system logos remain large enough to identify.
- TOP cards show system logo, system name, ROM count, and fallback initials so
  they never look empty when artwork is missing.
- Graphic TOP directional input follows the layout: `tile_grid` uses Left/Right
  for horizontal movement and Up/Down for vertical movement, while `tile_strip`
  primarily uses Left/Right movement. Page boundaries move naturally to the
  previous/next page.
- Graphic TOP page transitions are presentation-only theme settings under
  `graphic_mode.transition`. The currently supported values are `none` and
  `slide`. `slide` is eased vertical motion: the next page rises from below and
  the previous page descends from above. Themes may not change input, page size,
  confirm, or back behavior. Transition duration, axis, and easing are controlled
  by theme `transition_ms`, `transition_axis`, and `transition_easing`.
- ROM lists put the selectable list on the left and a selected-ROM preview panel
  on the right. One page shows 8 entries.
- ROM names prefer FreeType rendering so Japanese names and symbols remain
  accurate.
- ROM previews use `media.thumbnail` from the existing ROM cache. PNG
  thumbnails are drawn into the right preview panel with contain fitting; missing
  or unreadable artwork falls back to the initials panel.
- Graphic-mode ROM list scans use `--with-thumbnails`; the FE scan cache owns
  the ROM-to-thumbnail-path mapping through `media.thumbnail`.
- ROM preview title/detail text inherits the ROM-list rules. Preserve the
  original ROM name casing and symbols, compose decomposed kana dakuten/
  handakuten only at display time, scroll long selected text after the same
  `1000ms` marquee delay, and do not show row numbers, debug/status text, or a
  vertical bar as a cursor.
- JPEG/WEBP remain part of scanner lookup, but the Graphic renderer initially
  prioritizes PNG rendering.
- TOP/ROM/Favorites/Recent keep the same orange left accent bar as Text mode.
- Use a near-black neutral background, dark gray/teal cards, and orange selection
  outlines. Avoid one-note palettes such as all-purple, all-blue, or all-beige.
- Graphic mode also keeps all text at `1x` or larger. Primary labels use `2x`;
  large fallback initials may use about `4x` as visual identifiers.

## Interaction And Rendering Performance

- TOP and ROM lists should feel similar during Up/Down key repeat. ROM lists
  use TTF/FreeType and are naturally heavier than TOP, but obvious stutter
  during navigation should be treated as a problem.
- Keep TOP system-list rendering on the lightweight fixed-cell bitmap font.
  Do not casually replace the primary TOP system list with a proportional TTF,
  because that risks breaking fixed columns and smooth key repeat.
- ROM lists prioritize exact name display and consistent row spacing, so render
  the whole ROM name with TTF/FreeType. Do not return to a mixed style where
  ASCII uses the bitmap font and only non-ASCII uses TTF.
- In ROM-list FreeType rendering, avoid expensive work for every character of
  every row on every frame. Cache glyph advances, and normally measure text
  width only for the selected row where marquee decisions are needed.
- Draw FreeType glyphs as horizontal spans for contiguous pixels on the same
  row, not as one rectangle per pixel. This keeps ROM-list key repeat closer to
  TOP in perceived smoothness.
- Long selected ROM names wait `1000ms` before horizontal scrolling. Scrolling
  uses time-based pixel offsets rather than character steps; the current target
  speed is about `80px/sec`.
- Reset the marquee timer when moving the cursor, paging, entering a ROM list,
  or entering Favorites/Recent, so text does not start moving immediately when
  it becomes selected.
- TTY periodic redraw is for prompt-cursor blinking and ROM-name marquee.
  Input-driven screen updates should render immediately after input handling,
  not wait for the periodic redraw.
- Up/Down software key repeat is owned by the FE and must not depend on physical
  input-repeat events. The current baseline is `350ms` initial delay and `95ms`
  repeat interval. If those values change, compare TOP and ROM-list navigation
  on the real device.
- When evaluating smoothness, first confirm there is only one `/dev/fb0` owner.
  Duplicate FE processes can be mistaken for rendering or input problems.

## Verification

- After UI changes, keep a capture from `scripts/capture-a30-framebuffer.sh`.
- In captures, check for text below `1x`, overlap, column drift, and unwanted
  debug/status text.
- Before device review, confirm old `plumos-controller-ui-mali.bin` processes
  are not running in parallel. Use `scripts/stop-a30-display-processes.sh` to
  stop `/dev/fb0` owners before starting a fresh UI when needed.
