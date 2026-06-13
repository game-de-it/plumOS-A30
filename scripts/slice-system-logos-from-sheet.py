#!/usr/bin/env python3
"""Slice per-system TOP logos from a generated logo sheet."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw


THEME_IDS = ["default", "default-horizontal", "default-vertical"]

# ChatGPT Image 2026-06-13 sheet: 10 columns, 9 visual rows, 2048x768.
# The source contains text labels below each icon, so these y windows isolate
# only the icon bands before x-range detection runs.
ICON_Y_WINDOWS = [
    (8, 78),
    (108, 178),
    (206, 268),
    (304, 360),
    (386, 454),
    (478, 542),
    (566, 626),
    (652, 698),
    (724, 748),
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def read_system_order(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return [row["system_id"] for row in csv.DictReader(handle, delimiter="\t")]


def foreground(pixel: tuple[int, int, int]) -> bool:
    return max(pixel) > 24


def detect_icon_ranges(image: Image.Image, y0: int, y1: int, expected: int) -> list[tuple[int, int]]:
    active_columns: list[int] = []
    for x in range(image.width):
        count = 0
        for y in range(y0, y1):
            if foreground(image.getpixel((x, y))):
                count += 1
        if count >= 2:
            active_columns.append(x)

    ranges: list[tuple[int, int]] = []
    if active_columns:
        start = previous = active_columns[0]
        for x in active_columns[1:]:
            if x - previous > 5:
                ranges.append((start, previous))
                start = x
            previous = x
        ranges.append((start, previous))

    merged: list[tuple[int, int]] = []
    for start, end in ranges:
        if merged and start - merged[-1][1] < 28:
            merged[-1] = (merged[-1][0], end)
        else:
            merged.append((start, end))

    if len(merged) != expected:
        raise ValueError(
            f"detected {len(merged)} icon ranges, expected {expected}, y={y0}-{y1}: {merged}"
        )
    return merged


def connected_components(image: Image.Image) -> list[tuple[int, int, int, int, int, float]]:
    pixels = image.load()
    width, height = image.size
    mask = [[foreground(pixels[x, y]) for x in range(width)] for y in range(height)]
    seen = [[False] * width for _ in range(height)]
    components: list[tuple[int, int, int, int, int, float]] = []

    for y in range(height):
        for x in range(width):
            if not mask[y][x] or seen[y][x]:
                continue
            queue = [(x, y)]
            seen[y][x] = True
            xs: list[int] = []
            ys: list[int] = []
            max_values: list[int] = []
            for qx, qy in queue:
                xs.append(qx)
                ys.append(qy)
                max_values.append(max(pixels[qx, qy]))
                for nx in (qx - 1, qx, qx + 1):
                    for ny in (qy - 1, qy, qy + 1):
                        if (
                            0 <= nx < width
                            and 0 <= ny < height
                            and mask[ny][nx]
                            and not seen[ny][nx]
                        ):
                            seen[ny][nx] = True
                            queue.append((nx, ny))
            if len(xs) >= 3:
                components.append(
                    (
                        min(xs),
                        min(ys),
                        max(xs) + 1,
                        max(ys) + 1,
                        len(xs),
                        sum(max_values) / len(max_values),
                    )
                )
    return components


def trim_logo(image: Image.Image) -> Image.Image:
    width, height = image.size
    kept: list[tuple[int, int, int, int, int, float]] = []
    for component in connected_components(image):
        x0, y0, x1, y1, count, average_max = component
        component_height = y1 - y0
        edge_text_like = (
            component_height <= 12
            and count <= 320
            and average_max >= 85
            and (y0 >= height - 18 or y1 <= 13)
        )
        if not edge_text_like:
            kept.append(component)

    if not kept:
        kept = connected_components(image)
    if not kept:
        return image

    left = max(0, min(component[0] for component in kept) - 5)
    top = max(0, min(component[1] for component in kept) - 5)
    right = min(width, max(component[2] for component in kept) + 6)
    bottom = min(height, max(component[3] for component in kept) + 6)
    return image.crop((left, top, right, bottom))


def slice_logos(image: Image.Image, system_order: list[str]) -> dict[str, Image.Image]:
    if image.size != (2048, 768):
        raise ValueError(f"unexpected sheet size {image.size}; expected 2048x768")

    ranges_by_row: list[list[tuple[int, int]]] = []
    for row, (y0, y1) in enumerate(ICON_Y_WINDOWS):
        expected = 5 if row == 8 else 10
        ranges_by_row.append(detect_icon_ranges(image, y0, y1, expected))

    logos: dict[str, Image.Image] = {}
    for index, system_id in enumerate(system_order):
        row = index // 10
        column = index % 10
        y0, y1 = ICON_Y_WINDOWS[row]
        x0, x1 = ranges_by_row[row][column]
        crop = image.crop(
            (
                max(0, x0 - 8),
                max(0, y0 - 2),
                min(image.width, x1 + 9),
                min(image.height, y1 + 3),
            )
        )
        crop = trim_logo(crop)
        logos[system_id] = crop.resize(
            (crop.width * 2, crop.height * 2), Image.Resampling.NEAREST
        )
    return logos


def save_preview(images: dict[str, Image.Image], output: Path) -> None:
    cell_width = 230
    cell_height = 150
    columns = 5
    rows = math.ceil(len(images) / columns)
    preview = Image.new("RGB", (cell_width * columns, cell_height * rows), (2, 5, 5))
    draw = ImageDraw.Draw(preview)

    for index, (system_id, image) in enumerate(images.items()):
        fitted = image
        if fitted.width > cell_width - 18 or fitted.height > 112:
            scale = min((cell_width - 18) / fitted.width, 112 / fitted.height)
            fitted = fitted.resize(
                (max(1, int(fitted.width * scale)), max(1, int(fitted.height * scale))),
                Image.Resampling.NEAREST,
            )
        x = (index % columns) * cell_width + (cell_width - fitted.width) // 2
        y = (index // columns) * cell_height + 5 + (112 - fitted.height) // 2
        preview.paste(fitted, (x, y))
        draw.text(
            ((index % columns) * cell_width + 8, (index // columns) * cell_height + 125),
            system_id,
            fill=(240, 240, 240),
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    preview.save(output)


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheet", type=Path, help="Source 2048x768 logo sheet.")
    parser.add_argument(
        "--map",
        type=Path,
        default=root / "docs/libretro-system-core-map.tsv",
        help="TSV whose system_id order matches the sheet.",
    )
    parser.add_argument(
        "--theme-root",
        type=Path,
        default=root / "package/frontend/plumos/themes",
        help="Frontend theme root.",
    )
    parser.add_argument(
        "--preview",
        type=Path,
        default=root / "artifacts/logo-slice-preview/new-system-slice-preview.png",
        help="Preview image for generated logos.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing per-system logos. Default writes only missing files.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image = Image.open(args.sheet).convert("RGB")
    system_order = read_system_order(args.map)
    logos = slice_logos(image, system_order)
    written: dict[str, Image.Image] = {}

    for theme_id in THEME_IDS:
        logo_dir = args.theme_root / theme_id / "logos/systems"
        logo_dir.mkdir(parents=True, exist_ok=True)
        for system_id, logo in logos.items():
            output = logo_dir / f"{system_id}.png"
            if output.exists() and not args.overwrite:
                continue
            logo.save(output)
            written.setdefault(system_id, logo)

    if written:
        save_preview(written, args.preview)
    print(f"sheet_systems={len(logos)} written={len(written)} preview={args.preview}")
    for system_id, logo in written.items():
        print(f"  {system_id} {logo.width}x{logo.height}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
