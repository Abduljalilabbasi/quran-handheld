# Rebuild display assets

The ready-made SD ZIP is the simplest installation route. To rebuild, use Python 3.10+ and Pillow/requests:

```sh
python -m pip install -r tools/requirements.txt
python tools/convert_png.py --png-dir PATH_TO_FULL_PAGE_PNGS --output build/sdcard
python tools/verify_sd.py build/sdcard
```

The input directory must contain complete numbered pages `001.png` through `604.png`. Portrait preserves the entire source composition. Landscape reproduces the prototype's split algorithm. Files are generated separately; input PNGs are not modified.

## Fetch and render SVGs instead

The supplied browser-render tool comes from this project's original asset-building workflow. It uses Chrome/Edge headless rendering, not an SVG library that mishandles transparent hitboxes. Its automatic browser discovery targets Windows.

```sh
python tools/render_pages.py --test
python tools/render_pages.py --full
python tools/convert_png.py --png-dir build/cache_browser_png --output build/sdcard
```

The first command previews three sections of page 003. Inspect them before the full run. The full run downloads/renders 604 SVGs and may take considerable time. Its extra landscape output is under `build/browser_full`; `convert_png.py` creates the combined card layout.

Downloads default to the upstream revision recorded in the tool. Set `QURAN_SOURCE_REV` to a deliberately selected upstream commit to update it. The shipped prototype cache predates this recorded revision and cannot be claimed identical without hash comparison.

To compare a rebuild with the shipped release:

```sh
python tools/verify_sd.py build/sdcard --manifest docs/source-manifest.json
```

A mismatch may reflect changed upstream artwork or rasterizer versions. Review before replacing assets. Hashes verify identity, not Quran textual accuracy.

The asset source is SVG; the PNGs are browser-rendered intermediates. The reader consumes packed monochrome BIN files, not PNG files directly.

