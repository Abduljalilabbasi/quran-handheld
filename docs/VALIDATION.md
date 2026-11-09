# Validation — v0.2.0

Both responsive versions were compiled locally with ESP32 core 3.3.12 and GxEPD2 1.6.9 using `esp32:esp32:esp32s3:FlashSize=8M,PSRAM=opi,CDCOnBoot=cdc`.

| Firmware | Program storage | Global RAM | Software checks |
|---|---:|---:|---|
| Waveshare responsive | 442,924 bytes | 73,200 bytes | 6 state and 4 button checks |
| CrowPanel responsive | 403,424 bytes | 105,632 bytes | 7 state and 3 button checks |

The state/debounce method bodies were evaluated as compile-time C++ tests, with constexpr qualifiers and injected clock/level inputs for the harness. Cases include repeated taps during a captured refresh, mixed directions, bounds, menu transitions, blank/restore, long holds and wake-held release handling. These checks do not simulate physical scheduling, electrical bounce or panel timing.

The new [Actions workflow](../.github/workflows/compile.yml) compiles both board variants. Its actual run status is visible in the repository's Actions tab; do not infer a CI pass merely from a local build.

## Image verification

CrowPanel: 604 pages, 5 segments each, 3,020 files; each is 26,928 bytes. Total image data: 81,322,560 bytes. All checksums, dimensions, white header rows and ink counts were checked. Every black pixel in each final scaled/thresholded source page is present exactly once across its segments. The converter was smoke-tested for reproducibility. Representative pages and difficult boundaries were visually inspected. [Manifest](crowpanel-manifest.json).

Waveshare: the existing 1,810 landscape and 604 portrait BIN collection is reused. No image bytes were changed for responsive navigation. Historical provenance is in [source-manifest.json](source-manifest.json).

Pixel conservation is not complete linguistic proofreading. CrowPanel segmentation uses geometric assignment of small marks near boundaries; its correctness across every mark on every page has not been certified. Keep the source PNGs as reference and report any suspicious boundary.

## Hardware status

The owner previously reported working Waveshare reading and power-button blank/restore, and tested the CrowPanel sample enough to identify control/refresh issues. The new responsive updates and latest SD-power fix have compiled, but their complete on-device behavior has not been confirmed in this release. They are published as a **pre-release** for testing.

Test repeated presses while refresh is busy, reversals, bounds, page selection, blank/restore and saved position on both boards. On Waveshare, also test portrait and idle sleep/wake. On CrowPanel, test SD mounting and GPIO42 power enable. The old Windows compiler-startup issue is historical and no longer blocks local builds.
