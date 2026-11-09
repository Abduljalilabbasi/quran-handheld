# Responsive navigation

Both board firmwares keep collecting joystick and button input while the e-paper refreshes. Use the correct board guide: [Waveshare](WAVESHARE.md) or [CrowPanel](CROWPANEL.md). The details below describe CrowPanel; Waveshare retains its orientation menu, battery functions and idle sleep.

## Upload

Extract this ZIP and open `CrowPanel_Quran_Full/CrowPanel_Quran_Full.ino`. Keep **ReaderState.h in the same sketch folder**. Upload with the same ESP32S3 Dev Module, 8MB flash, OPI PSRAM and USB CDC settings as before. Libraries remain GxEPD2 1.6.9, Adafruit GFX and BusIO, using ESP32 core 3.3.12. No new library installation is needed; FreeRTOS comes with the ESP32 core.

## Behavior

- Every debounced joystick press/release updates the requested position, including during screen refresh. No button state is discarded after refreshing.
- One press still means one segment. Ten NEXT presses advance ten segments (two complete five-part Mushaf pages), subject to the 001/604 limits.
- The requested position is accumulated directly; there is no fixed-length page-turn queue to fill up.
- After 180 milliseconds without another state-changing input, the renderer takes the latest requested state. While it is busy, new presses keep changing the next requested state.
- An already-started e-paper refresh must finish. After it finishes, intermediate requested positions are skipped and the latest settled position is displayed. Pauses longer than the 180ms interval can allow intermediate refreshes.
- Mixed forward/back presses apply in order. Holding a direction still counts once on release; this update does not add automatic repetition.
- Centre hold opens the page menu using the **requested** position, even if the screen has not reached it yet. Centre click cycles units/tens/hundreds in the menu, and does nothing while reading. Hold again applies the page number.
- EXIT blanks/restores. Direction presses while blanked are ignored. A blank request during an active refresh is applied after that refresh finishes and input settles.
- The auxiliary MENU button remains reserved. No physical key changes brightness or triggers a full refresh.
- Page number location, image scale, partial-refresh waveform and 180-degree landscape orientation are retained.

## Technical notes and validation

A high-priority FreeRTOS task polls buttons every 5ms, with 20ms debounce. It only updates a small ReaderState under a short critical section. Only the Arduino loop accesses the display, SD, artwork buffers and Preferences. The state lock is never held during those operations.

The renderer records the revision of the snapshot it actually drew, so a request arriving during refresh cannot be accidentally marked as already displayed. There is no display reinitialization or hibernation between normal updates. Startup and the explicit Serial `f` command still use full refresh.

The last successfully loaded reading position is saved after navigation settles and two seconds have elapsed, or when blanking after it has been loaded. If you blank before a pending target is loaded, that target remains in RAM for restoration; an immediate hard power loss can restore the older saved position. A failed SD read does not replace the previous artwork; further navigation follows the requested position and the page menu can retry.

State-transition checks cover ten forward presses during a captured refresh, forward/back reversal, both bounds, pending-position menu entry, digit selection and application, invalid pages, blank/restore and inactive reading clicks. These checks run the firmware's state-method bodies as compile-time C++ evaluations (adding constexpr qualifiers for the harness). They do not simulate ESP32 scheduling or physical button timing. On-device responsiveness and panel behavior still need testing.

Serial Monitor at 115200: `n` next, `p` previous, `m` hold/menu/apply, `c` click, `b` blank, `f` full clean. Physical buttons are sampled independently; Serial commands are consumed by the Arduino loop between display updates.
