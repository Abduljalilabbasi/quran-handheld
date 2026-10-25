# SPDX-License-Identifier: GPL-3.0-or-later
"""
Quran 3.97" — SVG browser-render repair
=======================================

This keeps the ORIGINAL quran-ws SVG pages, but removes the renderer that caused
the black rectangles.

The previous full build used:
    raw SVG -> svglib/reportlab -> PNG
That renderer is the regression.

This version uses your installed Chrome/Edge browser to render the SVG exactly
as a browser does. Transparent ayah hit regions therefore stay transparent.

It then uses the SAME basic image logic as the successful test:
    render -> grayscale -> LANCZOS resize -> threshold -> packed 1-bit bitmap.

QUICK TEST FIRST:
    py rebuild_svg_with_browser.py --test

That rebuilds ONLY Mushaf page 003 into three screens and creates:
    browser_test\\preview\\page_003_part_1.png
    browser_test\\preview\\page_003_part_2.png
    browser_test\\preview\\page_003_part_3.png
    browser_test\\quran\\00001.bin
    browser_test\\quran\\00002.bin
    browser_test\\quran\\00003.bin

If those previews match the successful test, run:
    py rebuild_svg_with_browser.py --full

The full build reuses cache_svg\\001.svg ... cache_svg\\604.svg from your old
project if present. Put this script in the OLD Quran_397_SD_FullProject folder
to reuse those downloads.

Python dependencies:
    py -m pip install pillow requests
"""

from pathlib import Path
import argparse
import csv
import math
import os
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

import requests
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parent.parent / "build"
ROOT.mkdir(parents=True, exist_ok=True)
CACHE_SVG = ROOT / "cache_svg"
CACHE_BROWSER = ROOT / "cache_browser_png"

CACHE_SVG.mkdir(parents=True, exist_ok=True)
CACHE_BROWSER.mkdir(parents=True, exist_ok=True)

SOURCE_REV = os.environ.get("QURAN_SOURCE_REV", "b4155f07e4aea087d2c458069acff9f7c7e749f6")
RAW_BASE = f"https://raw.githubusercontent.com/quran-ws/quran-svg/{SOURCE_REV}/mushafs/hafs/kfqc/svg"

OUT_W = 800
OUT_H = 480
SCREEN_BYTES = OUT_W * OUT_H // 8

# Render at 2x final horizontal resolution before downsampling.
RENDER_W = 1600

# Same threshold used by the successful 3-part test.
BW_THRESHOLD = 230

# Keep the same special handling we had already chosen.
SPECIAL_PAGES = {1, 2}
SPECIAL_PARTS = 2
NORMAL_PARTS = 3

SESSION = requests.Session()
SESSION.headers.update({"User-Agent": "Quran397SVGRepair/1.0"})


def find_browser() -> Path:
    """Find Chrome or Edge already installed on Windows."""
    candidates = []

    env = os.environ

    for base in (
        env.get("PROGRAMFILES"),
        env.get("PROGRAMFILES(X86)"),
        env.get("LOCALAPPDATA"),
    ):
        if not base:
            continue

        b = Path(base)

        candidates.extend([
            b / "Google" / "Chrome" / "Application" / "chrome.exe",
            b / "Microsoft" / "Edge" / "Application" / "msedge.exe",
        ])

    for p in candidates:
        if p.exists():
            return p

    # Fallback to PATH.
    for name in ("chrome.exe", "msedge.exe", "chrome", "microsoft-edge"):
        found = shutil.which(name)
        if found:
            return Path(found)

    raise RuntimeError(
        "Chrome/Edge was not found. Install Google Chrome or Microsoft Edge, "
        "then run the script again."
    )


def download_svg(page_num: int) -> Path:
    path = CACHE_SVG / f"{page_num:03d}.svg"

    if path.exists() and path.stat().st_size > 10000:
        return path

    url = f"{RAW_BASE}/{page_num:03d}.svg"
    print(f"Downloading SVG page {page_num:03d}...")

    r = SESSION.get(url, timeout=120)
    r.raise_for_status()

    path.write_bytes(r.content)
    time.sleep(0.04)

    return path


def svg_viewbox_size(svg_path: Path):
    """Return SVG logical width,height from viewBox."""
    root = ET.parse(svg_path).getroot()

    vb = root.attrib.get("viewBox")
    if vb:
        vals = vb.replace(",", " ").split()
        if len(vals) == 4:
            return float(vals[2]), float(vals[3])

    def number(v):
        if not v:
            return None
        out = ""
        for c in v:
            if c.isdigit() or c in ".-+eE":
                out += c
            else:
                break
        return float(out) if out else None

    w = number(root.attrib.get("width"))
    h = number(root.attrib.get("height"))

    if w and h:
        return w, h

    # Standard page fallback ratio.
    return 345.0, 550.0


def render_svg_in_browser(svg_path: Path, browser: Path) -> Image.Image:
    """
    Render local SVG in Chrome/Edge, not svglib/reportlab.

    The browser correctly respects fill-opacity=0 on the ayah hit-test polygons.
    """
    cached = CACHE_BROWSER / f"{svg_path.stem}.png"

    if cached.exists() and cached.stat().st_size > 10000:
        return Image.open(cached).convert("L")

    vb_w, vb_h = svg_viewbox_size(svg_path)
    render_h = max(1, int(round(RENDER_W * vb_h / vb_w)))

    # Chrome accepts a large viewport; give one extra pixel to avoid clipping.
    viewport_w = RENDER_W
    viewport_h = render_h + 2

    svg_uri = svg_path.resolve().as_uri()

    html = f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
html, body {{
  margin: 0;
  padding: 0;
  background: white;
  overflow: hidden;
}}
img {{
  display: block;
  width: {RENDER_W}px;
  height: auto;
  margin: 0;
  padding: 0;
}}
</style>
</head>
<body>
<img src="{svg_uri}">
</body>
</html>
"""

    with tempfile.TemporaryDirectory(prefix="quran397_") as td:
        td = Path(td)
        html_path = td / "render.html"
        shot_path = td / "shot.png"

        html_path.write_text(html, encoding="utf-8")

        cmd = [
            str(browser),
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            "--allow-file-access-from-files",
            "--force-device-scale-factor=1",
            f"--window-size={viewport_w},{viewport_h}",
            f"--screenshot={shot_path}",
            html_path.resolve().as_uri(),
        ]

        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=60,
        )

        if result.returncode != 0 or not shot_path.exists():
            # Older Chrome/Edge may not understand --headless=new.
            cmd[1] = "--headless"
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=60,
            )

        if not shot_path.exists():
            raise RuntimeError(
                "Browser failed to render SVG.\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        img = Image.open(shot_path).convert("RGB")

        # Keep exactly the intended SVG viewport, discarding the spare rows.
        img = img.crop((0, 0, min(RENDER_W, img.width), min(render_h, img.height)))
        img.save(cached)

    return Image.open(cached).convert("L")


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


def pack_black_bits(img: Image.Image) -> bytes:
    """
    IDENTICAL polarity to the successful header test:
        black Pillow pixel -> set bit to 1
        white Pillow pixel -> bit stays 0
    """
    img = img.convert("1")
    px = img.load()
    data = bytearray()

    for y in range(img.height):
        for xb in range((img.width + 7) // 8):
            b = 0

            for bit in range(8):
                x = xb * 8 + bit

                if x < img.width and px[x, y] == 0:
                    b |= 1 << (7 - bit)

            data.append(b)

    return bytes(data)


def clear_old_output(quran_dir: Path):
    quran_dir.mkdir(parents=True, exist_ok=True)

    for p in quran_dir.glob("*.bin"):
        p.unlink()

    for name in ("count.txt", "manifest.csv"):
        p = quran_dir / name
        if p.exists():
            p.unlink()


def build(pages, output_base: Path, test_mode: bool):
    browser = find_browser()
    print(f"Browser renderer: {browser}")

    quran_dir = output_base / "quran"
    preview_dir = output_base / "preview"

    quran_dir.mkdir(parents=True, exist_ok=True)
    preview_dir.mkdir(parents=True, exist_ok=True)

    clear_old_output(quran_dir)

    screen_index = 1

    with (quran_dir / "manifest.csv").open(
        "w", newline="", encoding="utf-8"
    ) as mf:
        wr = csv.writer(mf)
        wr.writerow([
            "screen_index",
            "page",
            "part",
            "parts_total",
            "filename",
        ])

        for page_num in pages:
            print(f"\nPAGE {page_num:03d}/604")

            svg = download_svg(page_num)
            gray = render_svg_in_browser(svg, browser)

            if page_num in SPECIAL_PAGES:
                source = crop_actual_ink(gray)
                parts_total = SPECIAL_PARTS
            else:
                # Do NOT auto-crop normal pages. Keeping full SVG geometry is
                # what keeps font scale uniform from one page to the next.
                source = gray
                parts_total = NORMAL_PARTS

            parts = make_fixed_parts(source, parts_total)

            for part_no, img in enumerate(parts, start=1):
                raw = pack_black_bits(img)

                if len(raw) != SCREEN_BYTES:
                    raise RuntimeError(
                        f"Bad raw size page {page_num}, part {part_no}: {len(raw)}"
                    )

                filename = f"{screen_index:05d}.bin"
                (quran_dir / filename).write_bytes(raw)

                wr.writerow([
                    screen_index,
                    page_num,
                    part_no,
                    parts_total,
                    filename,
                ])

                # Test mode saves all previews. Full mode saves first pages.
                if test_mode or page_num <= 4:
                    img.save(
                        preview_dir /
                        f"page_{page_num:03d}_part_{part_no}.png"
                    )

                print(
                    f"  saved {filename} "
                    f"(part {part_no}/{parts_total})"
                )

                screen_index += 1

    total = screen_index - 1

    (quran_dir / "count.txt").write_text(
        f"{total}\n",
        encoding="utf-8",
    )

    print("\n====================================")
    print("DONE")
    print(f"Total screens: {total}")
    print(f"Quran folder:  {quran_dir}")
    print(f"Previews:      {preview_dir}")
    print("====================================")


def main():
    ap = argparse.ArgumentParser()

    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--test", action="store_true")
    mode.add_argument("--full", action="store_true")

    args = ap.parse_args()

    if args.test:
        # One normal page -> exactly 3 test screens.
        build(
            pages=[3],
            output_base=ROOT / "browser_test",
            test_mode=True,
        )
    else:
        build(
            pages=range(1, 605),
            output_base=ROOT / "browser_full",
            test_mode=False,
        )


if __name__ == "__main__":
    main()
