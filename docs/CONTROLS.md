# Controls

Both readers sample buttons independently while the display refreshes. Each debounced press/release counts; direction holds do not auto-repeat. After a 180ms input pause, the renderer catches up to the latest requested position. A refresh already underway must finish first. No fixed-size page-turn queue or ten-press limit is used.

| Action | Waveshare 3.97-inch | CrowPanel 5.79-inch |
|---|---|---|
| Forward | LEFT, GPIO6 | Joystick UP, GPIO6 |
| Backward | RIGHT, GPIO4 | Joystick DOWN, GPIO4 |
| Centre short while reading | No action | No action |
| Centre hold 0.9s then release | Page/orientation menu; hold again to apply | Page menu; hold again to apply |
| Centre short in menu | Units → tens → hundreds → orientation | Units → tens → hundreds |
| Direction in menu | Change digit or orientation | Change digit |
| Power/screen short | Power key GPIO1, active HIGH | EXIT GPIO1, active LOW |
| Power/screen in menu | Close menu and blank | Close menu and blank |
| Auxiliary | BOOT held 5s while blanked resets AXP2101 gauge | MENU GPIO2 reserved |
| Idle sleep | After 3 minutes; deferred while menu/open input/pending refresh | No automatic sleep |

In landscape, one press moves one segment. Waveshare portrait mode moves one complete Mushaf page. Invalid menu numbers outside 001–604 cannot be applied. Arrows are ignored while blanked; centre hold can open the menu from blank.

The page selector starts from the latest requested position, even if the screen is still finishing an older refresh. Blank/restore requests are also handled after an in-progress physical refresh finishes. Waveshare long power holds are left to the PMIC hardware.

Details: [Waveshare](WAVESHARE.md) · [CrowPanel](CROWPANEL.md) · [Responsive implementation](RESPONSIVE_NAVIGATION.md).
