# Installation — choose the correct board

| Board | Detailed guide | Firmware folder | Companion header | SD image folders |
|---|---|---|---|---|
| Waveshare ESP32-S3-ePaper-3.97 | [Waveshare](WAVESHARE.md) | `firmware/Quran_Reader_DualOrientation` | `WaveState.h` | `quran`, `quran_portrait` |
| Elecrow CrowPanel 5.79-inch | [CrowPanel](CROWPANEL.md) | `firmware/CrowPanel_Quran_Full` | `ReaderState.h` | `quran579` |

Download the matching firmware and SD ZIP from [v0.2.0](https://github.com/Abduljalilabbasi/quran-handheld/releases/tag/v0.2.0). Extract the SD ZIP before copying its folders to the card root. Use FAT32; back up before formatting. Existing working SD collections can be retained. Do not rename PNG files to BIN or use one board's images on the other.

## Arduino dependencies

- ESP32 core **3.3.12** by Espressif Systems.
- GxEPD2 **1.6.9**.
- Adafruit GFX Library **1.13.0** and Adafruit BusIO **1.17.4** (CI pins).
- SD/SD_MMC, SPI, Wire, Preferences and FreeRTOS come with the ESP32 core.

Use **ESP32S3 Dev Module**. The current verification builds use **8MB flash**, default partition scheme, **OPI PSRAM**, and **USB CDC On Boot enabled** with hardware CDC/JTAG USB mode. This describes the tested build layout, not a claim that both boards have identical physical flash capacity. Preserve your known-working Waveshare upload settings if they differ; the older Waveshare build also fitted the 4MB default partition layout without PSRAM.

Keep the companion header alongside the `.ino`. Select the actual USB port and upload. Avoid erase-all-flash when preserving the saved reading position.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GxEPD2@1.6.9" "Adafruit GFX Library@1.13.0" "Adafruit BusIO@1.17.4"
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PSRAM=opi,CDCOnBoot=cdc" firmware/Quran_Reader_DualOrientation
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PSRAM=opi,CDCOnBoot=cdc" firmware/CrowPanel_Quran_Full
```

## Initial checks

Cold startup can flash while establishing the display baseline. Test ordinary turns, rapid repeated presses, reversals and screen blank/restore. Check the part/page indicator and saved position after power-off. On Waveshare, also check portrait mode and wake after the 3-minute idle sleep. On CrowPanel, verify that the card mounts and page 001 through 604 can be selected; GPIO42 must enable the SD supply.

The [validation record](VALIDATION.md) distinguishes compilation and software checks from physical testing. Hardware vendor setup links are in the board guides and [hardware reference](HARDWARE.md).
