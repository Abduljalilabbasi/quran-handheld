# Design decisions

## Offline and physical controls

Focused Quran reading without network/account dependency is the purpose. E-paper retains the page while the MCU sleeps. No measured battery-runtime claim is made.

## Pre-rendered artwork

Source artwork supplies the Arabic shaping and page composition. The ESP32 reads packed images, avoiding runtime SVG/PNG decoding or font layout. Both modes use a 48,000-byte framebuffer.

Portrait preserves the complete composition, proportionally fitted, centred and monochrome-converted without reflow or cropping. Landscape enlarges text by splitting pages into consecutive sections. Scaling necessarily changes pixels; the panel cannot show the high-resolution PNG byte-for-byte.

## Orientation and overlays

Landscape is rotation 2 (180Â°), portrait rotation 3. Menus use logical display dimensions. Portrait deliberately has no page/battery overlay. Landscape retains the prototype's overlays; all-page overlap review is still pending.

## Refresh and sleep

Fast partial-update waveforms are used over the full screen area for page changes and white blank/restore. A full-area update is not a full-refresh waveform. Less flashing can mean more ghosting.

After three minutes, MCU deep sleep saves power while the screen image remains. On a recognized button wake, GxEPD2 initializes with `initial=false` so it does not force a new full cleaning cycle. Display power and RAM must remain intact. Cold boot uses normal initialization.

## State

The canonical position is the landscape screen index. Portrait moves to the first index of the next/previous Mushaf page. NVS stores position and orientation; RTC memory retains blank state across deep sleep.

The side power key owns blank/restore. Slider clicks have no reading-screen action, and cycle fields in the menu. Hardware long-press power-off remains. The power-button feature changes no PMIC charge current/voltage configuration.



## Responsive input in v0.2.0

Both board variants sample controls in an independent FreeRTOS task. It changes only a small requested-state structure protected by a short critical section. Rendering and hardware I/O remain on the main task. After a 180ms pause, the display takes a snapshot and draws that position; requests arriving during the physical refresh remain pending. No queue of intermediate screen images is loaded or rendered. A started physical refresh cannot be cancelled.

The board-specific implementations intentionally retain different SD buses, power polarity, display dimensions and sleep behavior. They live in separate firmware folders in one repository, with separate SD release assets. Shared artwork attribution and project documentation stay together.
