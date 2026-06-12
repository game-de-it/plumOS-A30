#!/usr/bin/env python3
"""Generate pixel-style fallback logos for the default Graphic themes."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SYSTEMS = [
    "atari2600",
    "atari7800",
    "doom",
    "gameandwatch",
    "lynx",
    "neogeocd",
    "odyssey2",
    "pokemini",
    "supergrafx",
    "supervision",
    "vectrex",
    "virtualboy",
]

VIRTUAL_TOP_IDS = [
    "favorites",
    "recent",
]

THEME_IDS = ["default", "default-horizontal", "default-vertical"]
BG = (2, 5, 5)
BLACK = (9, 10, 10)
DARK = (22, 24, 24)
MID = (55, 59, 59)
LIGHT = (172, 178, 172)
WHITE = (230, 233, 220)
RED = (190, 33, 30)
GREEN = (92, 198, 72)
BLUE = (64, 130, 210)
YELLOW = (230, 188, 44)
ORANGE = (230, 108, 38)
PURPLE = (80, 58, 170)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def font() -> ImageFont.ImageFont:
    return ImageFont.load_default()


def rect(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], fill, outline=None) -> None:
    draw.rectangle(box, fill=fill, outline=outline)


def bevel(
    draw: ImageDraw.ImageDraw,
    box: tuple[int, int, int, int],
    fill,
    outline=MID,
    hi=(116, 124, 124),
    lo=(11, 12, 12),
) -> None:
    x0, y0, x1, y1 = box
    rect(draw, box, fill, outline)
    draw.line((x0 + 1, y0 + 1, x1 - 1, y0 + 1), fill=hi)
    draw.line((x0 + 1, y0 + 1, x0 + 1, y1 - 1), fill=hi)
    draw.line((x0 + 1, y1 - 1, x1 - 1, y1 - 1), fill=lo)
    draw.line((x1 - 1, y0 + 1, x1 - 1, y1 - 1), fill=lo)


def screen(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], fill=(72, 92, 74)) -> None:
    bevel(draw, box, fill, (122, 132, 122), (164, 184, 154), (20, 32, 24))
    x0, y0, x1, _ = box
    rect(draw, (x0 + 3, y0 + 3, x1 - 3, y0 + 4), tuple(min(255, c + 34) for c in fill))
    rect(draw, (x0 + 3, y0 + 5, x0 + 4, y0 + 13), tuple(min(255, c + 18) for c in fill))


def pixel_text(
    draw: ImageDraw.ImageDraw,
    xy: tuple[int, int],
    text: str,
    fill=WHITE,
    anchor: str | None = None,
) -> None:
    draw.text(xy, text, font=font(), fill=fill, anchor=anchor)


def make_canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGB", (112, 80), BG)
    return image, ImageDraw.Draw(image)


def add_shadow(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = box
    rect(draw, (x0 + 3, y0 + 3, x1 + 3, y1 + 3), (0, 0, 0))


def speaker(draw: ImageDraw.ImageDraw, x: int, y: int, count: int = 5) -> None:
    for i in range(count):
        rect(draw, (x + i * 6, y, x + i * 6 + 2, y + 3), (88, 88, 82))


def draw_dpad(draw: ImageDraw.ImageDraw, x: int, y: int, color=MID) -> None:
    rect(draw, (x + 3, y, x + 6, y + 10), color)
    rect(draw, (x, y + 3, x + 10, y + 6), color)
    rect(draw, (x + 4, y + 4, x + 5, y + 5), BLACK)


def draw_buttons(draw: ImageDraw.ImageDraw, coords: list[tuple[int, int]], colors: list[tuple[int, int, int]]) -> None:
    for idx, (x, y) in enumerate(coords):
        fill = colors[idx % len(colors)]
        rect(draw, (x, y, x + 4, y + 4), fill)
        rect(draw, (x + 1, y + 1, x + 3, y + 2), tuple(min(255, c + 45) for c in fill))


def atari2600() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (14, 23, 98, 58))
    bevel(d, (14, 23, 98, 58), (34, 31, 27), (88, 88, 82), (82, 78, 70), BLACK)
    rect(d, (18, 27, 62, 52), (118, 68, 32), (56, 36, 22))
    for y in range(31, 49, 5):
        rect(d, (21, y, 59, y + 1), (158, 91, 38))
    for x in range(23, 59, 10):
        rect(d, (x, 29, x + 2, 50), (94, 54, 27))
    bevel(d, (64, 28, 92, 51), BLACK, (82, 88, 88), (112, 118, 116), (8, 8, 8))
    for x in range(67, 88, 6):
        rect(d, (x, 31, x + 2, 45), MID)
    rect(d, (72, 53, 79, 57), RED)
    rect(d, (85, 53, 91, 57), RED)
    rect(d, (47, 55, 63, 58), (20, 20, 18))
    pixel_text(d, (35, 16), "ATARI", WHITE, "mm")
    pixel_text(d, (78, 40), "2600", WHITE, "mm")
    bevel(d, (85, 61, 94, 70), BLACK, MID, (84, 88, 88), (3, 3, 3))
    rect(d, (88, 55, 90, 61), BLACK, MID)
    rect(d, (86, 53, 92, 56), RED)
    return scale(im)


def atari7800() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (13, 24, 99, 57))
    bevel(d, (13, 24, 99, 57), (28, 30, 32), (99, 104, 104), (112, 116, 116), BLACK)
    rect(d, (18, 29, 94, 36), (92, 92, 90))
    rect(d, (18, 39, 94, 43), RED)
    rect(d, (18, 44, 94, 46), (16, 16, 16))
    rect(d, (23, 49, 35, 52), MID)
    rect(d, (73, 49, 87, 52), MID)
    rect(d, (38, 48, 70, 51), (16, 18, 18))
    pixel_text(d, (56, 17), "ATARI", WHITE, "mm")
    pixel_text(d, (57, 42), "7800", WHITE, "mm")
    bevel(d, (19, 60, 33, 72), BLACK, MID, (80, 86, 86), (5, 5, 5))
    draw_dpad(d, 21, 60)
    draw_buttons(d, [(85, 60), (92, 60)], [RED, ORANGE])
    return scale(im)


def doom() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (20, 16, 92, 61))
    bevel(d, (20, 16, 92, 61), (32, 34, 34), (104, 104, 98), (116, 124, 112), BLACK)
    screen(d, (26, 22, 86, 53), (12, 20, 14))
    pixel_text(d, (56, 33), "DOOM", (250, 54, 28), "mm")
    rect(d, (43, 46, 69, 50), (90, 92, 84))
    rect(d, (48, 43, 63, 46), (172, 172, 158))
    rect(d, (37, 62, 75, 67), (36, 38, 36), (86, 86, 80))
    rect(d, (31, 67, 82, 71), (20, 20, 20))
    rect(d, (45, 57, 68, 59), (7, 8, 7))
    return scale(im)


def gameandwatch() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (21, 15, 91, 62))
    bevel(d, (21, 15, 91, 62), (202, 124, 36), (248, 194, 66), (250, 178, 48), (94, 50, 14))
    screen(d, (29, 23, 83, 50), (45, 56, 44))
    pixel_text(d, (56, 37), "G&W", (36, 82, 44), "mm")
    draw_dpad(d, 30, 52, (78, 54, 36))
    draw_buttons(d, [(72, 54), (81, 54)], [RED, RED])
    rect(d, (25, 19, 87, 21), (245, 158, 39))
    rect(d, (21, 62, 91, 64), (72, 38, 12))
    return scale(im)


def lynx() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (12, 21, 100, 59))
    bevel(d, (12, 21, 100, 59), (28, 32, 34), (100, 106, 108), (70, 76, 78), (5, 6, 6))
    rect(d, (16, 25, 28, 29), (62, 66, 66))
    rect(d, (84, 25, 96, 29), (62, 66, 66))
    screen(d, (36, 27, 76, 52), (78, 111, 72))
    pixel_text(d, (56, 17), "LYNX", WHITE, "mm")
    draw_dpad(d, 19, 35)
    draw_buttons(d, [(83, 34), (90, 39), (82, 45)], [RED, YELLOW, BLUE])
    rect(d, (15, 59, 97, 61), (4, 5, 5))
    return scale(im)


def neogeocd() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (18, 19, 94, 61))
    bevel(d, (18, 19, 94, 61), (202, 205, 198), (245, 245, 232), WHITE, (90, 94, 90))
    bevel(d, (29, 25, 79, 53), (162, 166, 164), (90, 94, 94), (210, 216, 206), (80, 84, 82))
    bevel(d, (39, 30, 69, 48), (24, 27, 28), (82, 86, 88), (60, 64, 66), BLACK)
    pixel_text(d, (54, 34), "NEO", YELLOW, "mm")
    pixel_text(d, (54, 43), "CD", YELLOW, "mm")
    rect(d, (24, 55, 88, 58), (120, 126, 124))
    rect(d, (30, 59, 82, 61), (86, 90, 88))
    draw_buttons(d, [(82, 26), (88, 26)], [BLUE, RED])
    return scale(im)


def odyssey2() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (14, 24, 97, 61))
    bevel(d, (14, 24, 97, 61), (222, 213, 182), (245, 238, 210), WHITE, (112, 104, 80))
    bevel(d, (21, 31, 59, 43), BLACK, (82, 88, 84), (72, 82, 76), (5, 5, 5))
    pixel_text(d, (40, 37), "O2", GREEN, "mm")
    speaker(d, 22, 49, 9)
    bevel(d, (67, 30, 89, 42), (55, 58, 56), (110, 112, 108), (90, 94, 92), (18, 20, 20))
    bevel(d, (82, 52, 92, 64), BLACK, MID, (86, 88, 86), (5, 5, 5))
    rect(d, (86, 45, 88, 52), BLACK, MID)
    rect(d, (84, 43, 91, 46), RED)
    pixel_text(d, (56, 18), "ODYSSEY", WHITE, "mm")
    return scale(im)


def pokemini() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (35, 12, 77, 66))
    bevel(d, (35, 12, 77, 66), (62, 174, 96), (150, 225, 150), (136, 238, 142), (18, 94, 48))
    screen(d, (42, 20, 70, 43), (72, 95, 78))
    pixel_text(d, (56, 32), "MINI", (33, 53, 40), "mm")
    draw_dpad(d, 40, 49, (36, 78, 48))
    draw_buttons(d, [(64, 50), (70, 55)], [RED, YELLOW])
    rect(d, (47, 15, 63, 17), (126, 220, 130))
    rect(d, (38, 66, 75, 68), (16, 80, 40))
    pixel_text(d, (56, 72), "POKEMON", WHITE, "mm")
    return scale(im)


def supergrafx() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (14, 21, 99, 61))
    bevel(d, (14, 21, 99, 61), (27, 29, 31), (90, 94, 94), (78, 84, 84), (4, 5, 5))
    bevel(d, (25, 29, 59, 52), BLACK, (64, 70, 72), (50, 58, 60), (4, 4, 4))
    bevel(d, (65, 30, 89, 50), (18, 21, 23), (70, 75, 78), (52, 56, 58), (4, 5, 5))
    pixel_text(d, (42, 39), "SGX", BLUE, "mm")
    rect(d, (21, 56, 94, 59), (50, 54, 54))
    rect(d, (25, 60, 60, 62), (10, 11, 11))
    bevel(d, (73, 62, 98, 72), BLACK, MID, (80, 84, 84), (4, 4, 4))
    draw_dpad(d, 77, 63)
    draw_buttons(d, [(92, 64), (98, 64)], [RED, BLUE])
    return scale(im)


def supervision() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (20, 15, 92, 64))
    bevel(d, (20, 15, 92, 64), (74, 75, 80), (145, 150, 154), (126, 130, 134), (22, 23, 26))
    screen(d, (35, 22, 77, 48), (73, 92, 77))
    pixel_text(d, (56, 35), "SV", (35, 67, 38), "mm")
    draw_dpad(d, 27, 50)
    draw_buttons(d, [(76, 51), (84, 51)], [RED, BLUE])
    rect(d, (23, 62, 90, 65), (18, 18, 20))
    pixel_text(d, (56, 10), "SUPERVISION", WHITE, "mm")
    return scale(im)


def vectrex() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (27, 9, 85, 68))
    bevel(d, (27, 9, 85, 68), (26, 27, 29), (96, 100, 102), (80, 84, 86), (5, 5, 6))
    screen(d, (35, 17, 77, 50), (8, 18, 16))
    pixel_text(d, (56, 31), "VECT", GREEN, "mm")
    pixel_text(d, (56, 40), "REX", GREEN, "mm")
    bevel(d, (34, 55, 78, 62), (16, 18, 18), MID, (65, 68, 68), (3, 3, 3))
    draw_buttons(d, [(40, 57), (49, 57), (58, 57), (67, 57)], [RED, BLUE, YELLOW, GREEN])
    rect(d, (28, 68, 84, 70), (4, 4, 4))
    return scale(im)


def virtualboy() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (19, 17, 93, 50))
    bevel(d, (19, 17, 93, 50), (32, 14, 18), (95, 45, 55), (92, 28, 36), (6, 3, 4))
    bevel(d, (27, 24, 85, 43), (68, 14, 22), (170, 38, 46), (120, 26, 34), (18, 5, 8))
    bevel(d, (37, 29, 53, 38), (16, 8, 10), RED, (120, 20, 25), BLACK)
    bevel(d, (59, 29, 75, 38), (16, 8, 10), RED, (120, 20, 25), BLACK)
    rect(d, (52, 50, 60, 68), (32, 26, 26), MID)
    rect(d, (42, 67, 70, 71), (34, 34, 34), MID)
    rect(d, (47, 71, 65, 73), (7, 7, 7))
    pixel_text(d, (56, 13), "VIRTUAL BOY", WHITE, "mm")
    return scale(im)


def favorites() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (18, 19, 94, 63))
    bevel(d, (18, 19, 94, 63), (32, 35, 37), (98, 104, 104), (88, 94, 94), (5, 6, 6))
    bevel(d, (25, 27, 87, 55), (18, 29, 30), (78, 88, 88), (82, 104, 104), (4, 5, 5))
    star = [
        (56, 25),
        (61, 35),
        (73, 35),
        (64, 42),
        (68, 54),
        (56, 47),
        (44, 54),
        (48, 42),
        (39, 35),
        (51, 35),
    ]
    d.polygon(star, fill=YELLOW)
    d.line(star + [star[0]], fill=(132, 92, 18))
    rect(d, (53, 33, 58, 38), (255, 230, 92))
    rect(d, (56, 48, 59, 51), ORANGE)
    rect(d, (23, 58, 89, 61), (16, 17, 18))
    rect(d, (31, 63, 81, 66), (9, 10, 11))
    draw_buttons(d, [(79, 22), (86, 22)], [RED, BLUE])
    pixel_text(d, (56, 13), "FAVORITES", WHITE, "mm")
    pixel_text(d, (56, 69), "FAV", (220, 226, 214), "mm")
    return scale(im)


def recent() -> Image.Image:
    im, d = make_canvas()
    add_shadow(d, (18, 17, 94, 64))
    bevel(d, (18, 17, 94, 64), (31, 33, 35), (94, 100, 102), (82, 90, 92), (5, 6, 6))
    screen(d, (27, 25, 85, 56), (20, 39, 43))
    d.ellipse((42, 27, 70, 55), fill=(12, 25, 28), outline=BLUE)
    d.ellipse((45, 30, 67, 52), outline=(126, 214, 200))
    rect(d, (55, 33, 57, 42), WHITE)
    rect(d, (57, 41, 65, 43), WHITE)
    rect(d, (52, 40, 60, 45), (118, 216, 196))
    rect(d, (55, 42, 57, 44), BLACK)
    d.arc((37, 22, 75, 60), 205, 508, fill=GREEN)
    rect(d, (38, 40, 42, 43), GREEN)
    rect(d, (40, 37, 43, 40), GREEN)
    speaker(d, 25, 59, 10)
    draw_buttons(d, [(78, 20), (85, 20)], [YELLOW, RED])
    pixel_text(d, (56, 12), "RECENT", WHITE, "mm")
    pixel_text(d, (56, 70), "HISTORY", (220, 226, 214), "mm")
    return scale(im)


def scale(image: Image.Image) -> Image.Image:
    scaled = image.resize((224, 160), Image.Resampling.NEAREST)
    return crop_to_content(scaled)


def crop_to_content(image: Image.Image) -> Image.Image:
    pix = image.load()
    width, height = image.size
    xs: list[int] = []
    ys: list[int] = []
    for y in range(height):
        for x in range(width):
            if pix[x, y] != BG:
                xs.append(x)
                ys.append(y)
    if not xs:
        return image
    pad = 10
    left = max(0, min(xs) - pad)
    top = max(0, min(ys) - pad)
    right = min(width, max(xs) + pad + 1)
    bottom = min(height, max(ys) + pad + 1)
    return image.crop((left, top, right, bottom))


GENERATORS = {
    "atari2600": atari2600,
    "atari7800": atari7800,
    "doom": doom,
    "gameandwatch": gameandwatch,
    "lynx": lynx,
    "neogeocd": neogeocd,
    "odyssey2": odyssey2,
    "pokemini": pokemini,
    "supergrafx": supergrafx,
    "supervision": supervision,
    "vectrex": vectrex,
    "virtualboy": virtualboy,
    "favorites": favorites,
    "recent": recent,
}


def save_preview(images: dict[str, Image.Image], output: Path) -> None:
    cell_w = 240
    cell_h = 190
    cols = 4
    rows = (len(images) + cols - 1) // cols
    preview = Image.new("RGB", (cell_w * cols, cell_h * rows), BG)
    draw = ImageDraw.Draw(preview)
    for idx, (system_id, image) in enumerate(images.items()):
        col = idx % cols
        row = idx // cols
        x = col * cell_w + (cell_w - image.width) // 2
        y = row * cell_h + 8 + (150 - image.height) // 2
        preview.paste(image, (x, y))
        pixel_text(draw, (col * cell_w + 8, row * cell_h + 166), system_id, (190, 198, 190))
    output.parent.mkdir(parents=True, exist_ok=True)
    preview.save(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--theme-root", type=Path, default=repo_root() / "package/frontend/plumos/themes")
    parser.add_argument(
        "--preview",
        type=Path,
        default=repo_root() / "artifacts/logo-slice-preview/missing-system-logos-preview.png",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generated: dict[str, Image.Image] = {}
    for system_id in SYSTEMS + VIRTUAL_TOP_IDS:
        generated[system_id] = GENERATORS[system_id]()

    for theme_id in THEME_IDS:
        logo_dir = args.theme_root / theme_id / "logos/systems"
        logo_dir.mkdir(parents=True, exist_ok=True)
        for system_id, image in generated.items():
            image.save(logo_dir / f"{system_id}.png")

    save_preview(generated, args.preview)
    for system_id, image in generated.items():
        print(f"generated {system_id} {image.width}x{image.height}")
    print(f"preview {args.preview}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
