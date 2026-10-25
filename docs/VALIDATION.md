# Validation and release status

## Available evidence

- Owner reported the idle-flash fix worked.
- Owner reported final power-button controls and fast white blank/restore worked.
- SD data: 1,810 landscape and 604 portrait images, 48,000 bytes each.
- Portrait packing was decoded to verify pixels; representative previews were inspected.
- The authoring environment could not start its ESP32 compiler (Windows DLL relocation error). The owner compiled and used the sketch separately. There is no recorded passing automated build yet.
- No measured battery runtime, enclosure fit test or complete human Quran proofread is recorded.

## Public-release checks

- Pass the supplied CI compile using pinned dependencies.
- Test pages 1, 2, 3, 300 and 604 in both orientations and navigation directions.
- Compare **all** rendered pages against an authorized Mushaf; inspect split boundaries and landscape overlays for obscured content. File checks are not textual certification.
- Confirm enclosure authorship/license, battery fit and assembly.
- Test short/long power presses on USB and battery, after sleep, while blank and visible.
- Test invalid page numbers, missing files, orientation persistence and menu cycling.

## Known limitations

The menu does not auto-sleep. Partial updates can ghost, and the refresh counter does not currently schedule a cleaning cycle. Landscape overlays have not been audited for all pages. Flash-free wake requires retained display power/RAM. Battery-gauge maintenance registers need revision checks on other hardware. This is a prototype, not a publisher-reviewed Quran device.

