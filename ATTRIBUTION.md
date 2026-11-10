# Sources and attribution

## Quran artwork

Original publisher: **King Fahd Glorious Quran Printing Complex (KFQC)**, Madinah Mushaf, Hafs edition.

Digital source: **[Quran.ws / quran-svg](https://github.com/quran-ws/quran-svg)**, [mushafs/hafs/kfqc/svg](https://github.com/quran-ws/quran-svg/tree/main/mushafs/hafs/kfqc/svg), pages 001â€“604.

This project does not claim authorship of the Quran, its typesetting or page artwork. Quran.ws's metadata and ayah-polygon contributions have their own terms; the underlying artwork retains the publisher's terms. Read the preserved [upstream notice](LICENSES/QURAN-SVG-NOTICE.md), [upstream license](LICENSES/QURAN-SVG-LICENSE.txt), and [publisher site](https://qurancomplex.gov.sa).

Attribution: **Quran page resource from Quran.ws (quran-ws/quran-svg), Hafs/KFQC edition; publisher artwork by the King Fahd Glorious Quran Printing Complex. Modified for display as below.** Quran.ws metadata contributions: CC BY 4.0. This is not a blanket CC BY claim over publisher artwork.

### Waveshare transformations and provenance

- Browser-render SVGs to respect transparent ayah-hit polygons.
- Portrait: complete cached PNG, proportionally fit inside 480 Ã— 800, centre on white, LANCZOS resize, threshold 230, packed 1-bit.
- Landscape: trim excess whitespace for pages 1â€“2 and split into two sections; preserve normal page geometry and split into three sections for pages 3â€“604.
- Output: row-major, MSB first, black=1, 48,000 bytes per screen, no header.
- No source file modification, reflow, AI text or reconstructed glyphs.

The SD pack uses the prototype's existing local cache. Its original download commit was not recorded. `docs/source-manifest.json` records cached SVG/PNG and shipped BIN hashes. The upstream commit observed during packaging was `b4155f07e4aea087d2c458069acff9f7c7e749f6`; this is **not claimed as the historical cache revision**. New downloads may differ and need revalidation.

## CrowPanel 5.79-inch transformations

The same cached KFQC/Quran.ws source PNGs are converted into five 792x272 segments per Mushaf page (3,020 files). Each BIN is 26,928 bytes, MSB-first, black=1, with 24 reserved white rows for the upper-left indicator. Each page is proportionally resized and thresholded at 230; its five segments use one scale. Connected ink components are kept whole, with geometric assignment of small marks near segment boundaries. This is automated image segmentation, not certified linguistic proofreading. No text is generated or retyped.

[The CrowPanel manifest](docs/crowpanel-manifest.json) records source PNG hashes and output hashes, scaling and ink counts. Every black pixel of each final thresholded page is checked to occur once across its five segments; this does not certify every mark's line assignment.

CrowPanel hardware and GPIO42 SD-power initialization follow [Elecrow's official board documentation](https://static-cdn.elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html) and [SD example](https://static-cdn.elecrow.com/wiki/CrowPanel_ESP32_E-Paper_5.79inch_Arduino_Tutorial.html#example-4-initialize-sd-card). The display dependency is GxEPD2's GDEY0579T93 driver. NumPy and SciPy support conversion and retain their upstream licenses.

## Software

Project firmware and tools: GPL-3.0-or-later. GxEPD2 by Jean-Marc Zingg uses GPL-3.0. Adafruit GFX/BusIO retain their BSD terms, and Arduino-ESP32 retains its upstream licenses. Dependencies are installed from upstream, not vendored.

- [GxEPD2](https://github.com/ZinggJM/GxEPD2)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)
- [Arduino-ESP32](https://github.com/espressif/arduino-esp32)
- [Pillow](https://python-pillow.org)

## Hardware and enclosure

Waveshare designed the board. This project links to its schematic and claims no ownership of that PCB.

The enclosure (`hardware/enclosure/`) was designed by Abduljalil Alabbasi and is licensed under [Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/). It contains a top cover, bottom case and two button parts. The CAD is not covered by the software GPL license.

## LilyGO T5S3 Pro port

Uses the existing Waveshare image collection. Landscape removes only empty outer margins and proportionally enlarges the remaining artwork within 960 × 504; portrait preserves the complete source page at 540 × 900. No text is generated, reflowed or retyped.

Hardware support follows [LilyGO's official Pro repository](https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO), pinned to `87436b5c9d0d4fc09f44ef43a4fb5d41a757494a`. The release includes its unchanged LGPL-3.0 epdiy library and license. Fira Sans comes from the same vendor example and retains its [SIL Open Font License](LICENSES/FiraSans-OFL.txt). The GT911 Home-key bit and GPIO11 light level follow the vendor driver/UI. Reader code retains GPL-3.0-or-later.
