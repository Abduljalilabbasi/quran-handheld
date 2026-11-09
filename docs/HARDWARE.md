# Waveshare hardware

| Item | Quantity | Notes |
|---|---:|---|
| Waveshare ESP32-S3-ePaper-3.97 | 1 | Integrated ESP32-S3, 800 Ã— 480 monochrome panel, SD and controls |
| microSD | 1 | FAT32, 8â€“32 GB is sufficient |
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
| IÂ²C SDA / SCL | 41 / 42 |

The schematic routes the power key through an inverting transistor to GPIO1. Firmware reads that signal directly and leaves the PMIC hard-off timing unchanged. GPIO1 wakes on EXT0 HIGH; slider keys wake on EXT1 LOW.

Firmware uses AXP2101-compatible battery registers at IÂ²C 0x34. Current vendor documentation calls the PMIC TG28 while vendor source uses AXP2101 naming. Check the physical revision before extending register-level power management.

- [Official board](https://docs.waveshare.com/ESP32-S3-ePaper-3.97)
- [Official schematic](https://files.waveshare.com/wiki/ESP32-S3-ePaper-3.97/ESP32-S3_e-Paper-3.97-schematic.pdf)
- [Arduino guide](https://docs.waveshare.com/ESP32-S3-ePaper-3.97/Arduino)
- [Vendor examples](https://github.com/waveshareteam/ESP32-S3-ePaper-3.97)

Mount without pressing/twisting the display. Keep SD, USB, power and slider accessible. Verify battery polarity and clearances before closing the enclosure. Tested screw lengths and enclosure fit are not yet established.



## CrowPanel 5.79-inch variant

Use the Elecrow CrowPanel ESP32-S3 5.79-inch 792x272 black/white board (dual SSD1683). Display: SCK12, MOSI11, CS45, DC46, RESET47, BUSY48, power-enable GPIO7 HIGH. SD SPI: SCK39, MISO13, MOSI40, CS10; **GPIO42 must be OUTPUT/HIGH before SD initialization**. GPIO6/4 navigate, GPIO5 is centre, GPIO1 is the active-low EXIT screen key, GPIO2 is reserved. There is no AXP2101 gauge port or automatic MCU sleep in this version.

[Elecrow hardware reference](https://static-cdn.elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html) · [SD initialization example](https://static-cdn.elecrow.com/wiki/CrowPanel_ESP32_E-Paper_5.79inch_Arduino_Tutorial.html#example-4-initialize-sd-card) · [CrowPanel setup](CROWPANEL.md).

The existing 3MF/STL enclosure is for Waveshare only. No CrowPanel CAD has been supplied.
