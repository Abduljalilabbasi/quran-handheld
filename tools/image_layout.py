# SPDX-License-Identifier: GPL-3.0-or-later
from PIL import Image, ImageOps
OUT_W=800
OUT_H=480
BW_THRESHOLD=230

def crop_actual_ink(gray: Image.Image) -> Image.Image:
    """
    Used only for pages 1-2, whose SVG viewBoxes are intentionally unusual.
    Normal pages keep full page geometry so their text scale stays consistent
    from page to page.
    """
    inv = ImageOps.invert(gray)
    mask = inv.point(lambda p: 255 if p > 8 else 0)
    bbox = mask.getbbox()

    if bbox is None:
        return gray

    l, t, r, b = bbox

    mx = max(6, int(gray.width * 0.008))
    my = max(6, int(gray.height * 0.008))

    return gray.crop((
        max(0, l - mx),
        max(0, t - my),
        min(gray.width, r + mx),
        min(gray.height, b + my),
    ))


def make_fixed_parts(source: Image.Image, num_parts: int):
    """
    This mirrors the successful test logic:
      - calculate ONE scale for the whole page
      - resize whole page ONCE
      - threshold after resizing
      - split the resized result
      - center each part in 800x480

    Therefore the three thirds cannot acquire different font sizes.
    """
    target_w = OUT_W - 12
    target_h = OUT_H - 8

    scale_w = target_w / source.width
    scale_h = target_h / (source.height / float(num_parts))
    scale = min(scale_w, scale_h)

    new_w = max(1, round(source.width * scale))
    new_h = max(1, round(source.height * scale))

    resized = source.resize((new_w, new_h), Image.Resampling.LANCZOS)

    bw = resized.point(
        lambda p: 0 if p < BW_THRESHOLD else 255
    ).convert("1")

    part_height = new_h / float(num_parts)

    screens = []

    for i in range(num_parts):
        top = round(i * part_height)
        bottom = round((i + 1) * part_height)

        part = bw.crop((0, top, new_w, bottom))

        canvas = Image.new("1", (OUT_W, OUT_H), 1)

        x = (OUT_W - part.width) // 2
        y = (OUT_H - part.height) // 2

        canvas.paste(part, (x, y))
        screens.append(canvas)

    return screens


