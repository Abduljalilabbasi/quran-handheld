# Quran reader for LilyGO T5S3 Pro 4.7-inch

Arduino IDE port for **H752-01 / TPS65185**, 960 x 540, ESP32-S3 N16R8. Not the older H752, standard T5 S3, Waveshare or CrowPanel pinout.

Download [LilyGO_T5S3_Pro_Quran.zip](https://github.com/Abduljalilabbasi/quran-handheld/releases/download/v0.3.0/LilyGO_T5S3_Pro_Quran.zip), which includes the matching display library.

## Install in Arduino IDE

1. Extract this package. Keep every file in `LilyGO_Quran_Reader` together. Open `LilyGO_Quran_Reader.ino`.
2. In Preferences, add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to Additional Boards Manager URLs. In Boards Manager install **esp32 by Espressif Systems 2.0.14** for this port. The Waveshare/CrowPanel builds used 3.3.12; select their version again when returning to those projects. This package was compiled using an isolated 2.0.14 installation, without replacing the existing 3.3.12 installation.
3. Copy the supplied `libraries/epdiy` directory into your sketchbook `libraries` directory. Restart the IDE after installing it. Use this LilyGO-patched copy, not stock epdiy or LilyGo-EPD47. No SensorLib or GxEPD2 dependency is needed by this sketch.
4. Select **ESP32S3 Dev Module**. Set Flash Size **16MB**, Partition Scheme **16M Flash (3MB APP/9.9MB FATFS)**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**, USB Mode **Hardware CDC and JTAG**, Flash Mode **QIO 80MHz**, CPU **240MHz**, Arduino and Events on **Core 1**. Leave Erase All Flash disabled. Select the LilyGO USB port and Upload.
5. If upload cannot connect, hold BOOT, press/release RST, release BOOT, then upload. RST is reset, not the screen-blank control.

## SD card

Use FAT32. Copy `quran` and `quran_portrait` directly into the card root. These are the existing **Waveshare** image folders, not CrowPanel's `quran579`.

The complete compatible SD ZIP is available at:
https://github.com/Abduljalilabbasi/quran-handheld/releases/download/v0.2.0/Waveshare_Quran_SD.zip

There are 1,810 landscape BINs and 604 portrait BINs. Each image is 48,000 bytes, MSB-first, black=1. The firmware expects the complete collection. Safely eject the card before moving it from the PC to the reader. Restart after inserting it.

## Controls

- **Circular capacitive Home button below the display**: tap once to toggle the frontlight on/off. Holding does not auto-repeat. Uses the vendor high setting (230/255). It starts off after restart and does not change the page or trigger refresh.

- Tap the **left third**: forward one segment, or one complete page in portrait. Tap the **right third**: backward. This preserves your swapped left/right preference.
- Hold the **centre third** for at least 900ms and release: open the page menu or apply the selection.
- Tap the centre in the menu: units, tens, hundreds, orientation. Left/right change the active field. A centre tap while reading has no effect.
- **BOOT** button: same centre click/hold behavior. Avoid holding BOOT through reset, unless entering upload mode.
- **PCA9535 user button (IO12)**: short press/release blanks everything, including page/battery labels; press again to restore. This is a software blank, not deep sleep or hardware power-off.
- Serial Monitor 115200: `n` next, `p` previous, `m` menu/apply, `c` next field, `b` blank/restore, `f` manual cleaning refresh.

Touch is sampled by a separate task while the main task reads SD and refreshes. After 180ms without a new request, the display catches up. No fixed ten-press limit. An in-progress refresh must finish. The latest successfully rendered position/orientation is saved after two seconds of settled input.

## Display

Pure black/white artwork and UI, using epdiy MODE_DU differential updates. A full cleaning cycle runs once at boot or when you send `f`. Blank/restore uses the same differential update. No periodic idle reinitialization is used.

Landscape removes only empty outer image margins and fits the remaining artwork proportionally into **960 x 504**, beneath a 36-pixel header. All source ink remains inside the crop; no Arabic is reflowed or regenerated. Tall segments can still have side margins because stretching or cutting the artwork would distort it. Portrait preserves the whole original page geometry, rendered at 540 x 900 with an outside header. The page/part number is at upper left; battery state of charge is at upper right when the fuel gauge responds. No indicator is drawn over the Quran artwork.

`Config.h` has `FLIP_180=true` to preserve your flipped mounting. Set false if needed. Optional touch-mirror settings are provided for hardware calibration. Panel VCOM follows LilyGO's example (1560mV magnitude); this is not a brightness adjustment. Frontlight starts off; only the separate circular Home button toggles it. Screen navigation gestures do not adjust brightness.

## Validation and limits

Compiled with ESP32 core 2.0.14; see the build result in the release package. Firmware was uploaded to the owner's device; serial confirmed GT911 touch initialization and successful SD image reads during navigation. The owner confirmed the circular Home button toggles the frontlight on/off. The enlarged landscape layout was compiled and uploaded. Local package checks verify every landscape crop preserves every source ink pixel and its scaled bounds fit the available display area. This is not a claim of complete physical or linguistic proofreading.

This first Pro port leaves the MCU awake (no automatic deep sleep). Panel high voltage is turned off after each refresh. GPS, LoRa, Wi-Fi and Bluetooth are not started. Battery percentage updates when the page is rendered; USB status and charge-gauge reset controls from the Waveshare are not ported. Use the board's own hardware power controls for extended storage. Broader touch alignment, rapid-input stress, blank/restore, orientation and ghosting checks remain to be completed.

## Source and licenses

Reader code: GPL-3.0-or-later, based on your quran-handheld Waveshare responsive state machine. Main project: https://github.com/Abduljalilabbasi/quran-handheld

Display driver and font were taken from LilyGO's official Pro repository, commit **87436b5c9d0d4fc09f44ef43a4fb5d41a757494a**:
https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO
The supplied epdiy library is unchanged and retains its LGPL-3.0 license in `libraries/epdiy/LICENSE`. It contains the H752-01 TPS65185/PCA9535 board support. The stock FastEPD T5PRO enum in that checkout refers to an older shift-register board; it is deliberately not used. Fira Sans retains the SIL Open Font License in `LICENSES/FiraSans-OFL.txt`.

Quran artwork: **King Fahd Glorious Quran Printing Complex**, accessed via **Quran.ws / quran-ws/quran-svg**, Hafs/KFQC edition. https://github.com/quran-ws/quran-svg/tree/main/mushafs/hafs/kfqc/svg
The existing binary collection is reused; only white margins are cropped and pixels enlarged for the screen. No Quran text is generated or retyped. Preserved upstream terms are in `LICENSES`. Full provenance: https://github.com/Abduljalilabbasi/quran-handheld/blob/main/ATTRIBUTION.md
