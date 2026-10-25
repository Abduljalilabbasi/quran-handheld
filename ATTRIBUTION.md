# Sources and attribution

## Quran artwork

Original publisher: **King Fahd Glorious Quran Printing Complex (KFQC)**, Madinah Mushaf, Hafs edition.

Digital source: **[Quran.ws / quran-svg](https://github.com/quran-ws/quran-svg)**, [mushafs/hafs/kfqc/svg](https://github.com/quran-ws/quran-svg/tree/main/mushafs/hafs/kfqc/svg), pages 001–604.

This project does not claim authorship of the Quran, its typesetting or page artwork. Quran.ws's metadata and ayah-polygon contributions have their own terms; the underlying artwork retains the publisher's terms. Read the preserved [upstream notice](LICENSES/QURAN-SVG-NOTICE.md), [upstream license](LICENSES/QURAN-SVG-LICENSE.txt), and [publisher site](https://qurancomplex.gov.sa).

Attribution: **Quran page resource from Quran.ws (quran-ws/quran-svg), Hafs/KFQC edition; publisher artwork by the King Fahd Glorious Quran Printing Complex. Modified for display as below.** Quran.ws metadata contributions: CC BY 4.0. This is not a blanket CC BY claim over publisher artwork.

### Transformations and provenance

- Browser-render SVGs to respect transparent ayah-hit polygons.
- Portrait: complete cached PNG, proportionally fit inside 480 × 800, centre on white, LANCZOS resize, threshold 230, packed 1-bit.
- Landscape: trim excess whitespace for pages 1–2 and split into two sections; preserve normal page geometry and split into three sections for pages 3–604.
- Output: row-major, MSB first, black=1, 48,000 bytes per screen, no header.
- No source file modification, reflow, AI text or reconstructed glyphs.

The SD pack uses the prototype's existing local cache. Its original download commit was not recorded. `docs/source-manifest.json` records cached SVG/PNG and shipped BIN hashes. The upstream commit observed during packaging was `b4155f07e4aea087d2c458069acff9f7c7e749f6`; this is **not claimed as the historical cache revision**. New downloads may differ and need revalidation.

## Software

Project firmware and tools: GPL-3.0-or-later. GxEPD2 by Jean-Marc Zingg uses GPL-3.0. Adafruit GFX/BusIO retain their BSD terms, and Arduino-ESP32 retains its upstream licenses. Dependencies are installed from upstream, not vendored.

- [GxEPD2](https://github.com/ZinggJM/GxEPD2)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)
- [Arduino-ESP32](https://github.com/espressif/arduino-esp32)
- [Pillow](https://python-pillow.org)

## Hardware and enclosure

Waveshare designed the board. This project links to its schematic and claims no ownership of that PCB.

`hardware/enclosure/esp32quranenclosure.3mf` was supplied by the project owner. It contains a top cover, bottom case and two button parts. Designer and license metadata are blank. CAD is excluded from the software license; confirm original authorship and redistribution terms before public distribution.
