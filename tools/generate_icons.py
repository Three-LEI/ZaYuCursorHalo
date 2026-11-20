#!/usr/bin/env python3
"""Generate CursorHalo multi-resolution Windows icons.

The app ships one icon group for the active state and one for paused. Windows
uses different sizes for the desktop, Explorer, Alt+Tab and the notification
area; every size in this list is rendered independently instead of being a
single 32 px bitmap stretched by the shell.
"""

from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "res"
ARTIFACTS = ROOT / "artifacts"
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

ACTIVE_TOP = (66, 132, 255)
ACTIVE_BOTTOM = (18, 48, 128)
PAUSED_TOP = (84, 94, 110)
PAUSED_BOTTOM = (30, 35, 46)


def blend(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        round(a[0] + (b[0] - a[0]) * t),
        round(a[1] + (b[1] - a[1]) * t),
        round(a[2] + (b[2] - a[2]) * t),
    )


def render(size: int, paused: bool) -> Image.Image:
    # Supersample. Small sizes need a larger factor so a 1.2 px ring and
    # pause bars still have clean edges after LANCZOS reduction.
    scale = 8 if size <= 64 else 4
    canvas = size * scale
    image = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    margin = max(1, round(size * 0.045)) * scale
    radius = max(2, round(size * 0.22)) * scale
    box = (margin, margin, canvas - margin, canvas - margin)

    top = ACTIVE_TOP if not paused else PAUSED_TOP
    bottom = ACTIVE_BOTTOM if not paused else PAUSED_BOTTOM
    gradient = Image.new("RGB", (2, 2))
    gradient.putpixel((0, 0), top)
    gradient.putpixel((1, 0), blend(top, bottom, 0.35))
    gradient.putpixel((0, 1), blend(top, bottom, 0.35))
    gradient.putpixel((1, 1), bottom)
    gradient = gradient.resize((canvas, canvas), Image.Resampling.BICUBIC)

    tile_mask = Image.new("L", (canvas, canvas), 0)
    ImageDraw.Draw(tile_mask).rounded_rectangle(box, radius=radius, fill=255)
    image.paste(gradient, (0, 0), tile_mask)

    # Subtle top rim: keeps the tile from disappearing into dark taskbars.
    draw.rounded_rectangle(
        box,
        radius=radius,
        outline=(255, 255, 255, 54 if not paused else 38),
        width=max(1, round(size * 0.004)) * scale,
    )

    cx = canvas // 2
    cy = canvas // 2
    ring_radius = max(4.0, size * 0.315) * scale
    ring_width = max(1.2, size * 0.070) * scale
    white = (255, 255, 255, 255) if not paused else (226, 232, 242, 255)
    draw.ellipse(
        (cx - ring_radius, cy - ring_radius, cx + ring_radius, cy + ring_radius),
        outline=white,
        width=round(ring_width),
    )

    if paused:
        bar_width = max(1.2, size * 0.052) * scale
        bar_height = max(2.8, size * 0.155) * scale
        gap = max(1.1, size * 0.050) * scale
        top_y = cy - bar_height / 2
        bottom_y = cy + bar_height / 2
        left_x = cx - gap / 2 - bar_width
        right_x = cx + gap / 2
        draw.rounded_rectangle(
            (left_x, top_y, left_x + bar_width, bottom_y),
            radius=bar_width / 2,
            fill=white,
        )
        draw.rounded_rectangle(
            (right_x, top_y, right_x + bar_width, bottom_y),
            radius=bar_width / 2,
            fill=white,
        )
    else:
        dot_radius = max(1.1, size * 0.067) * scale
        draw.ellipse(
            (cx - dot_radius, cy - dot_radius, cx + dot_radius, cy + dot_radius),
            fill=(238, 249, 255, 255),
        )

    return image.resize((size, size), Image.Resampling.LANCZOS)


def write_ico(path: Path, frames: list[Image.Image]) -> None:
    payloads = []
    for frame in frames:
        import io

        buffer = io.BytesIO()
        frame.save(buffer, format="PNG")
        payloads.append(buffer.getvalue())

    count = len(frames)
    offset = 6 + count * 16
    entries = []
    for size, payload in zip((frame.width for frame in frames), payloads):
        entries.append(
            struct.pack(
                "<BBBBHHII",
                0 if size == 256 else size,
                0 if size == 256 else size,
                0,
                0,
                1,
                32,
                len(payload),
                offset,
            )
        )
        offset += len(payload)

    with path.open("wb") as output:
        output.write(struct.pack("<HHH", 0, 1, count))
        for entry in entries:
            output.write(entry)
        for payload in payloads:
            output.write(payload)


def preview(active: dict[int, Image.Image], paused: dict[int, Image.Image]) -> None:
    canvas = Image.new("RGB", (920, 400), (242, 244, 248))
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, 460, 400), fill=(24, 28, 38))
    draw.text((20, 16), "CursorHalo active / paused", fill=(230, 235, 245))
    draw.text((480, 16), "Light desktop", fill=(40, 45, 55))
    x = 24
    for size in (128, 64, 48, 32, 24, 16):
        if size != 128:
            x += 128
        canvas.paste(active[size], (x, 70), active[size])
        canvas.paste(paused[size], (x, 240), paused[size])
        draw.text((x, 48), str(size), fill=(170, 180, 200))
        draw.text((x + 36, 238), "pause", fill=(170, 180, 200))
    canvas.save(ARTIFACTS / "p4-icon-preview.png")


def main() -> None:
    RES.mkdir(exist_ok=True)
    ARTIFACTS.mkdir(exist_ok=True)
    active = {size: render(size, False) for size in SIZES}
    paused = {size: render(size, True) for size in SIZES}

    active[256].save(RES / "tray_active.png")
    paused[256].save(RES / "tray_paused.png")
    write_ico(RES / "tray_active.ico", [active[size] for size in SIZES])
    write_ico(RES / "tray_paused.ico", [paused[size] for size in SIZES])
    preview(active, paused)
    print("Wrote res/tray_active.ico, res/tray_paused.ico, PNG previews")


if __name__ == "__main__":
    main()
