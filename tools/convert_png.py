# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert complete 001.png..604.png pages into both SD formats."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
from PIL import Image, ImageOps
from image_layout import crop_actual_ink, make_fixed_parts

def packed(image):
    raw = bytes(b ^ 255 for b in image.convert("1").tobytes())
    if len(raw) != 48000:
        raise ValueError("Expected exactly 48,000 bytes")
    return raw

def build(cache, output):
    for n in range(1, 605):
        if not (cache / f"{n:03d}.png").is_file():
            raise FileNotFoundError(f"Missing page {n:03d}.png")
    landscape = output / "quran"
    portrait = output / "quran_portrait"
    for folder in (landscape, portrait):
        folder.mkdir(parents=True, exist_ok=True)
    manifest = []
    index = 1
    with (landscape / "manifest.csv").open("w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(["screen_index", "page", "part", "parts_total", "filename"])
        for page in range(1, 605):
            source = cache / f"{page:03d}.png"
            with Image.open(source) as im:
                rgba = im.convert("RGBA")
            white = Image.new("RGBA", rgba.size, "white")
            white.alpha_composite(rgba)
            gray = white.convert("L")
            small = ImageOps.contain(gray, (480, 800), Image.Resampling.LANCZOS)
            canvas = Image.new("L", (480, 800), 255)
            canvas.paste(small, ((480-small.width)//2, (800-small.height)//2))
            bw = canvas.point(lambda p: 0 if p < 230 else 255).convert("1")
            data = packed(bw)
            (portrait / f"{page:05d}.bin").write_bytes(data)
            manifest.append({"page": page, "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest()})
            total = 2 if page <= 2 else 3
            section_source = crop_actual_ink(gray) if page <= 2 else gray
            for part, section in enumerate(make_fixed_parts(section_source, total), 1):
                name = f"{index:05d}.bin"
                (landscape / name).write_bytes(packed(section))
                wr.writerow([index, page, part, total, name])
                index += 1
    assert index == 1811
    (landscape / "count.txt").write_text("1810\n")
    (portrait / "count.txt").write_text("604\n")
    (output / "png-provenance.json").write_text(json.dumps(manifest, indent=2))
    print("Built 1,810 landscape sections and 604 complete portrait pages.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--png-dir", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/sdcard"))
    args = parser.parse_args()
    build(args.png_dir, args.output)

