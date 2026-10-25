# Hardware

| Item | Quantity | Notes |
|---|---:|---|
| Waveshare ESP32-S3-ePaper-3.97 | 1 | Integrated ESP32-S3, 800 × 480 monochrome panel, SD and controls |
| microSD | 1 | FAT32, 8–32 GB is sufficient |
| USB-C data cable | 1 | Upload and power |
| Single-cell 3.7 V lithium battery | Optional | Compatible MX1.25 plug; verify polarity and fit |
| Enclosure and fixings | Optional | Owner-supplied 3MF with case and buttons; fit unverified |

No custom PCB is needed. Other ThinV2/EasyEDA drafts are separate projects, not part of this release. Battery capacity and runtime have not been measured/documented.

| Function | GPIO |
|---|---|
| E-paper BUSY / DC / CS | 3 / 9 / 10 |
| E-paper SCLK / MOSI / RESET | 11 / 12 / 46 |
| SD CLK / CMD / D0 (1-bit) | 16 / 17 / 15 |
| Logical left / centre / right | 6 / 5 / 4 |
| BOOT | 0 |
| Power PWR_OUT | 1, active HIGH |
| I²C SDA / SCL | 41 / 42 |

The schematic routes the power key through an inverting transistor to GPIO1. Firmware reads that signal directly and leaves the PMIC hard-off timing unchanged. GPIO1 wakes on EXT0 HIGH; slider keys wake on EXT1 LOW.

Firmware uses AXP2101-compatible battery registers at I²C 0x34. Current vendor documentation calls the PMIC TG28 while vendor source uses AXP2101 naming. Check the physical revision before extending register-level power management.

- [Official board](https://docs.waveshare.com/ESP32-S3-ePaper-3.97)
- [Official schematic](https://files.waveshare.com/wiki/ESP32-S3-ePaper-3.97/ESP32-S3_e-Paper-3.97-schematic.pdf)
- [Arduino guide](https://docs.waveshare.com/ESP32-S3-ePaper-3.97/Arduino)
- [Vendor examples](https://github.com/waveshareteam/ESP32-S3-ePaper-3.97)

Mount without pressing/twisting the display. Keep SD, USB, power and slider accessible. Verify battery polarity and clearances before closing the enclosure. Tested screw lengths and enclosure fit are not yet established.

