# v0.2.0 — Waveshare and CrowPanel responsive readers

Both hardware variants now live in this repository with separate firmware and setup guides. This is a pre-release: local builds and software checks passed; the latest responsive behavior still needs on-device confirmation.

## Downloads

Choose the two ZIPs matching your board:

| Board | Firmware source | SD images |
|---|---|---|
| Waveshare ESP32-S3-ePaper-3.97 | Waveshare_Quran_Responsive.zip | Waveshare_Quran_SD.zip |
| Elecrow CrowPanel 5.79-inch | CrowPanel_Quran_Responsive.zip | CrowPanel_Quran_SD.zip |

Extract the SD ZIP and copy its folders directly to the root of a FAT32 microSD card. Waveshare uses `quran` and `quran_portrait`; CrowPanel uses `quran579`. Keep the companion header beside the sketch. Existing image collections remain compatible. SHA256SUMS.txt covers all four ZIPs.

## Changes

- Buttons remain active during display refresh. Each valid tap updates the requested position; after a 180 ms pause the display catches up to the latest request. There is no ten-press limit. One tap advances one landscape segment, or one whole page in Waveshare portrait mode.
- Waveshare retains portrait/landscape selection, page menu, blank/restore and idle sleep/wake behavior.
- CrowPanel includes all 604 pages in five segments each (3,020 screens), a reserved upper-left page indicator, menu controls and the GPIO42 SD-power enable fix.
- Source code, installation, wiring, controls, image conversion and validation are browsable in the repository. Existing Waveshare 3MF/STL enclosure files and photos are retained.

## Setup and validation

[Installation](https://github.com/Abduljalilabbasi/quran-handheld/blob/main/docs/INSTALLATION.md) · [Waveshare](https://github.com/Abduljalilabbasi/quran-handheld/blob/main/docs/WAVESHARE.md) · [CrowPanel](https://github.com/Abduljalilabbasi/quran-handheld/blob/main/docs/CROWPANEL.md) · [Validation](https://github.com/Abduljalilabbasi/quran-handheld/blob/main/docs/VALIDATION.md)

Both firmwares compiled locally with ESP32 core 3.3.12 and GxEPD2 1.6.9. State/button checks and archive integrity checks passed. Pixel conservation does not certify every Quran segment boundary; report any suspicious boundary and compare it with the source page.

Quran artwork: King Fahd Glorious Quran Printing Complex, accessed through [Quran.ws / quran-svg](https://github.com/quran-ws/quran-svg), `mushafs/hafs/kfqc/svg`. No Quran text is generated or retyped. [Attribution and licenses](https://github.com/Abduljalilabbasi/quran-handheld/blob/main/ATTRIBUTION.md).
