# Installation

## Prepare the hardware and SD

Use the exact Waveshare ESP32-S3-ePaper-3.97 and a USB-C data cable. An 8–32 GB microSD formatted FAT32 is straightforward. Back up before formatting.

Extract `Quran_SD_Card.zip` and copy its folders directly to the card root:

```text
SD:/
  quran/
    count.txt          # 1810
    manifest.csv
    00001.bin ... 01810.bin
  quran_portrait/
    count.txt          # 604
    00001.bin ... 00604.bin
```

Each BIN is 48,000 bytes. Do not nest the folders or rename PNG files to BIN. Safely eject and insert the card into the reader.

## Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Add the official ESP32 Boards Manager URL in Preferences if necessary:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install **esp32 by Espressif Systems 3.3.12** in Boards Manager.
4. In Library Manager, install **GxEPD2 1.6.9**, **Adafruit GFX Library 1.13.0**, and **Adafruit BusIO 1.17.4**.
5. Open `firmware/Quran_Reader_DualOrientation/Quran_Reader_DualOrientation.ino` inside its matching folder.
6. Select **ESP32S3 Dev Module**, the actual COM port, and the settings below.
7. Verify, then Upload.

| Setting | Recorded prototype setting |
|---|---|
| CPU | 240 MHz |
| Flash mode | QIO |
| Flash size / partition | 4 MB / Default |
| PSRAM | Disabled |
| USB mode | Hardware CDC and JTAG |
| USB CDC on boot | Disabled (FQBN default) |
| Upload speed | 921600; reduce if necessary |
| Erase all flash | Disabled |

The board is advertised with 16 MB flash and 8 MB PSRAM. **The 4 MB setting is the recorded build/partition layout, not the physical capacity.** The sketch fits that layout and allocates a 48 KB framebuffer without PSRAM.

If upload fails, consult the [Waveshare Arduino guide](https://docs.waveshare.com/ESP32-S3-ePaper-3.97/Arduino). For manual download mode, hold BOOT while resetting/powering on, then release BOOT and upload. Avoid erasing NVS when preserving the saved page.

## CLI equivalent

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GxEPD2@1.6.9" "Adafruit GFX Library@1.13.0" "Adafruit BusIO@1.17.4"
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled" firmware/Quran_Reader_DualOrientation
```

Use the IDE to upload, or `arduino-cli upload` with the same FQBN and your actual port. No network is needed by the handheld.

## Verify

At fresh power-on, a full refresh is expected. Test several pages; select portrait and inspect pages 001, 002 and 003. Test power-key blank/restore twice. Wait over three minutes and test wake. Confirm page and orientation survive power-off.

## Troubleshooting

- **SD ERROR:** mounting failed; check FAT32, contacts and seating.
- **FILE ERROR / MISSING PAGE:** check root folder placement, name and byte size.
- **Black blocks in rebuilt pages:** use a browser renderer respecting transparent SVG polygons.
- **Flash after idle:** keep wake-aware `display.init(115200, !wokeFromButton)`. Display power/RAM must survive sleep.
- **Faint ghosting:** fast updates trade cleanliness for less flashing. A cold boot triggers a full refresh; no automatic periodic cleaning cycle is currently invoked.
- **Compiler DLL relocation error:** this authoring environment could not start the Windows ESP32 compiler. Use Arduino IDE or the supplied CI build. It is not an SD problem.

