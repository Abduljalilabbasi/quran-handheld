# CrowPanel 5.79-inch Quran reader - complete landscape edition

All **604 Mushaf pages**, each divided into **five screens**: **3,020 SD image files**. Landscape only, 180-degree rotation. Uses the same partial-refresh GxEPD2 driver as the latest page-003 sample. Quran artwork is taken from the existing original KFQC/Quran.ws PNG collection, not retyped.

## Install

1. Extract the complete ZIP.
2. Use a FAT32 microSD card with at least 150 MB free. Back up any existing files before formatting; formatting is not necessary if the card already works and is FAT32.
3. Copy the **quran579** folder from **SD_CARD** directly to the SD card root. The result must be `SD:/quran579/001_1.bin`, through `SD:/quran579/604_5.bin`. Do not leave it nested inside SD_CARD or inside the ZIP. The BIN files are the display images; the reader does not load PNGs directly.
4. Insert the card in the CrowPanel's TF slot while the board is off.
5. Open `firmware/CrowPanel_Quran_Full/CrowPanel_Quran_Full.ino` in Arduino IDE. Keep ReaderState.h beside the sketch. Keep ReaderState.h beside it. This sketch replaces the one-page sample; do not add the old sample's EPD/spi files or embedded bitmap header.
6. Install **esp32 by Espressif Systems 3.3.12**, **GxEPD2 1.6.9**, **Adafruit GFX Library**, and **Adafruit BusIO**. SD, SPI and Preferences come with the ESP32 core.
7. Select **ESP32S3 Dev Module**, **Flash Size 8MB**, **PSRAM OPI PSRAM**, **USB CDC On Boot Enabled**, **USB Mode Hardware CDC and JTAG**, and your board's USB port. Upload.
8. Startup initializes the screen with full refresh and opens the last saved position (page 1 initially). Normal navigation, menus, blanking and restoration use partial refresh.

## Reading controls

| Control | Function |
|---|---|
| Joystick UP / GPIO6 | Next segment; after part 5, advances to next page part 1 |
| Joystick DOWN / GPIO4 | Previous segment; before part 1, returns to previous page part 5 |
| Centre short click | No action while reading |
| Centre hold 0.9 seconds, then release | Open page selector |
| EXIT / GPIO1 short click | Plain white screen / restore current Quran page |
| MENU / GPIO2 | Reserved, no action |

The last successfully loaded reading position is saved after navigation settles and two seconds have elapsed, or when blanking after that position has been loaded. An abrupt power loss within that 2-second interval can restore the previous saved segment. Arrows are ignored while blanked. Centre hold can open the selector while blanked. The two page-range ends stop without redundant refreshes.

The upper-left label is **part/page** at `(4,4)`, matching the original reader's location and order. For example, `2/125` means part 2 of Mushaf page 125. It uses text size 2 instead of the old size 4 so the short screen has more reading space. The first 24 pixel rows are reserved white space, so the label never covers Quran artwork. There are no battery or USB indicators, including when blanked.

## Original-style page selector

The selector opens on the current page with the **units** digit active. A short centre click cycles **units -> tens -> hundreds -> units**. Joystick UP increases that digit; DOWN decreases it, wrapping between 0 and 9. Hold centre and release to jump to **part 1** of the selected page. Valid pages are **001-604**; invalid numbers stay in the menu. EXIT closes the menu and blanks the screen. There is no orientation field.

## Image preparation and checks

- Each frame is 792x272 pixels, 26,928 bytes, row-major, MSB-first, **1 = black**.
- 81,322,560 bytes of image data in total (about 77.6 MiB, plus filesystem allocation).
- The converter resizes each full source page proportionally, then thresholds at 230. It starts at width 780 and slightly reduces the entire page's scale if needed to fit all five segments below the header. All five segments of a given page have the same scale.
- Pages 1 and 2 have unusually large blank margins. Only those margins are cropped before scaling. They still have five parts, including their headings.
- Normal pages use the three-line divisions calibrated from the sample. Connected strokes are kept whole. Small marks near a boundary are assigned using nearby glyph bodies with bounds checks. Their original shape and relative placement within each group are retained.
- Every black pixel in each final scaled/thresholded page is verified to appear exactly once in its five parts. Each binary is decoded and checked, and the 24px header is checked to be white. `docs/crowpanel-manifest.json` records original PNG hashes, scale, segment bounds, ink counts and output hashes.
- Representative pages and difficult boundaries were visually checked. This is an automated conversion, not a complete scholarly proofreading of all 604 pages. Pixel conservation verifies image integrity but cannot certify every linguistic mark's segment assignment. The original PNG collection remains the reference.

`previews/` contains selected five-part overview images. Preview labels use a desktop font; the device uses Adafruit GFX's built-in font. Preview captions are outside the actual screen area.

## Hardware and refresh

Display SPI uses SCK12, MOSI11, CS45, DC46, RESET47, BUSY48 and power-enable GPIO7. The SD card uses a separate SPI bus: SCK39, MISO13, MOSI40, CS10, with the SD supply enabled by GPIO42 HIGH before initialization. This is **SD over SPI**, not the old Waveshare SD_MMC wiring. Board pin reference: https://static-cdn.elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html

The driver is `GxEPD2_579_GDEY0579T93` for the 792x272 dual-SSD1683 panel. Current and previous display RAM are synchronized by GxEPD2. There is no automatic display reset, full cleaning cycle or MCU sleep after idle. Software blanking does not disconnect the battery. The original AXP2101 battery-gauge reset cannot be mapped directly to this board, so GPIO2 is reserved. Physical BOOT and RST retain their hardware functions.

At 115200 baud, Serial Monitor reports update duration and SD errors. Commands: `n` next, `p` previous, `m` menu/apply, `c` next digit, `b` blank/restore, `f` explicit full cleaning refresh. No physical button changes darkness or requests cleaning. Text is fixed black/white; actual contrast and ghosting depend on the panel.

If a file is missing or invalid, the old reading position is kept and an error is shown. Reinsert/correct the SD card, then hold centre and select the desired page to retry. A read is committed only after all 26,928 bytes arrive.

Compilation passed; physical SD operation and refresh quality still need testing on your CrowPanel. See [validation](VALIDATION.md). Sources and rights are in ATTRIBUTION.md and the included Quran notices.

## Rebuild the image collection

The ready-to-use SD images are already included. To regenerate them from your original PNGs:

```powershell
python -m pip install -r tools/requirements-crowpanel.txt
python tools/build_crowpanel_pages.py --source "path/to/cache_browser_png" --output build/crowpanel
```

The input folder must contain `001.png` through `604.png`. An optional `--output` directory avoids overwriting this package; `--pages 3 420 557` builds a selected subset. Source PNGs are not modified. The renderer does not download or retype Quran content.

## SD power fix

The revised sketch enables GPIO42 HIGH before starting the SD SPI bus, matching Elecrow's official TF example. The earlier full-reader sketch omitted this step and could report SD not found even with a FAT32 card. Re-upload this sketch; existing quran579 files are unchanged.

## Responsive navigation update

Buttons continue working during refresh. Each press updates the requested segment; after a 180ms input pause the screen catches up to the latest request, finishing any in-progress refresh first. See [responsive navigation](RESPONSIVE_NAVIGATION.md) for behavior and validation details.

See [responsive navigation](RESPONSIVE_NAVIGATION.md) for input handling during refresh. Download the firmware and CrowPanel SD pack from [v0.2.0](https://github.com/Abduljalilabbasi/quran-handheld/releases/tag/v0.2.0). The ZIP download named CrowPanel_Quran_SD.zip contains quran579 directly at its root.
