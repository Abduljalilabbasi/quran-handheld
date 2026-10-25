# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify an extracted SD package, optionally against the shipped hash manifest."""
from pathlib import Path
import argparse
import hashlib
import json
def verify(root, manifest=None):
    for name, count in (("quran", 1810), ("quran_portrait", 604)):
        folder = root / name
        assert (folder / "count.txt").read_text().strip() == str(count), f"{name}: count.txt"
        actual = {p.name for p in folder.glob("*.bin")}
        expected = {f"{n:05d}.bin" for n in range(1, count+1)}
        assert actual == expected, f"{name}: missing or unexpected binary names"
        for n in expected:
            assert (folder / n).stat().st_size == 48000, f"{name}/{n}: size"
    if manifest:
        for entry in json.loads(manifest.read_text())["outputs"]:
            assert hashlib.sha256((root / entry["file"]).read_bytes()).hexdigest() == entry["sha256"], entry["file"]
    print("All 2,414 screen files passed; this does not replace a visual Quran review.")
if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("sd_root", type=Path)
    p.add_argument("--manifest", type=Path)
    a = p.parse_args()
    verify(a.sd_root, a.manifest)

