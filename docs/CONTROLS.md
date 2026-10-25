# Controls

Left/right refer to the intended flipped landscape assembly: logical left is GPIO6 and right is GPIO4.

| Context | Action | Result |
|---|---|---|
| Reading | Left/right | Next/previous section in landscape, full page in portrait |
| Reading | Centre click | No screen action |
| Reading | Centre hold (~0.9 s) | Open page/orientation selector |
| Menu | Centre click | Units → tens → hundreds → orientation → units |
| Menu | Left/right | Change digit or toggle orientation |
| Menu | Centre hold | Apply page 001–604 and orientation |
| Reading / blank | Power short press | Plain white / restore Quran page |
| Menu | Power short press | Close menu and blank |
| Any | Power long hold | Existing PMIC hardware power control |

The menu is a **page-number selector**, not a selectable Juz/Hizb list, despite legacy naming in older versions. Changing orientation returns to the first section of the selected page.

After three minutes in the main-loop inactivity state, the ESP32 sleeps while e-paper retains its image. Power uses GPIO1/EXT0 wake and toggles the retained blank/visible state. Slider keys use EXT1. The menu has a blocking input loop, so it currently does not auto-sleep while open.

The old maintenance action is retained: while blank, holding BOOT for five seconds resets the battery gauge and briefly shows its confirmation. It is not a normal reading action.

