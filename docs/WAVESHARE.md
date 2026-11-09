# Waveshare Quran reader - responsive navigation

For **Waveshare ESP32-S3-ePaper-3.97**, using your existing DualOrientation reader and SD files. This is a separate firmware package from the CrowPanel version.

## Install

1. Extract this ZIP.
2. Open `firmware/Quran_Reader_DualOrientation/Quran_Reader_DualOrientation.ino`. Keep **WaveState.h beside it**.
3. Use your existing Waveshare Arduino board/upload settings and USB port. The verification build used ESP32S3 Dev Module, ESP32 core 3.3.12, 8MB flash, OPI PSRAM, USB CDC enabled, GxEPD2 1.6.9 and Adafruit GFX/BusIO.
4. Upload and restart. **Keep the existing Waveshare SD card contents**: `quran`, `quran/count.txt`, and `quran_portrait` if you use portrait mode. The CrowPanel `quran579` files are not for this board.

## What changed

A separate input task continues sampling buttons during e-paper refresh and SD reads. Each debounced press/release updates the requested position. After 180ms without another state-changing input, the display draws the latest requested position. A refresh already in progress must finish; intermediate requests accumulated during it are skipped afterward.

There is no fixed-size page-turn queue. Press once, twice, twenty times, or more: each valid press changes the requested position, stopping at the first/last available screen. Forward/back presses apply in order. Holding a direction counts once on release; this does not add auto-repeat. Short presses must be stable for the 20ms debounce interval.

- **Landscape:** one press advances one of your existing segments. Pages 1 and 2 have two segments each; pages 3-604 have three each.
- **Portrait:** one press advances one complete Mushaf page.
- **LEFT / GPIO6:** forward. **RIGHT / GPIO4:** backward.
- **Centre short:** no action while reading; menu cycles units -> tens -> hundreds -> orientation -> units.
- **Centre hold 0.9s and release:** open menu at the latest requested page, or apply its selected page/orientation.
- **Power short:** completely blank / restore. In the menu it closes the menu and blanks. Long power holds remain for the board's hardware power function.
- **BOOT hold 5s and release while blanked:** existing battery-gauge reset.

Screen blanking, menu updates and battery-gauge maintenance also leave input sampling running. The page/part indicator, battery/USB indicators while reading, original rotations, partial-refresh driver and artwork rendering are retained. Blanking contains no indicators.

## Sleep and saved position

The original 3-minute MCU idle sleep remains. It is deferred while navigation is pending, a button is held, or the page menu is open. Input sampling is stopped before changing the pins to RTC wake mode. The original wake handling is retained: a slider key wakes the reader, and the next press navigates; a short power wake toggles blank/restore. Input sampling starts after startup/wake initialization has completed.

Successfully loaded and displayed reading positions are saved using the original NVS namespace, so your saved location is retained when upgrading. Targets skipped during a navigation burst are not individually loaded or saved. If a pending target is blanked before being displayed and the board is then hard-powered off, the last successfully displayed/saved position is restored.

A missing SD image shows an error and does not overwrite the saved reading position. Navigation continues from the requested target; hold centre to choose/retry a page after correcting the SD files.

## Validation

The input task owns button debouncing and updates only a small state protected by a short critical section. Display, SD_MMC, I2C, NVS and rendering remain on the Arduino loop task. The state lock is never held across I/O or display waits. A finished refresh marks only its captured revision as displayed, leaving any newer input pending.

Six state checks cover 20 presses during a captured refresh, mixed directions and bounds, portrait mapping across the first two pages and page 604, the four-field menu, blank/gauge behavior, and invalid selections/activity tracking. Four button checks cover 20 taps, active-high power polarity, the five-second BOOT hold, and ignoring a held wake button's release. The firmware method bodies were evaluated by the cross-compiler with constexpr qualifiers and explicit clock/level inputs added for testing; these checks do not simulate physical FreeRTOS scheduling or panel timing.

See [validation](VALIDATION.md) for the Arduino compilation results. Physical behavior still needs testing on your Waveshare. The earlier firmware remains in Git history; the CrowPanel implementation lives beside it in this repository.
