# 06 — Input and autoswipe

**English** · [中文](../zh/06-input.md)

The real touch driver is not ported. The 0084 patch proves the HID path with a
kernel timer that calls the same `SweetHIDTouch::touch()` a real driver will
use.

## Path

```
kernel timer -> SweetHIDTouch::touch()
  -> IOHIDEventService -> IOHIDEventSystem
  -> backboardd -> SpringBoard
```

The timer is gated by boot args; without `sweetautoswipe`, nothing fires.

## Edge tip

| Item | Value | Note |
|---|---|---|
| Start Y | `2390` | inside the bottom 48 px edge zone |
| Zone check | `y > 2399 - 48` | sets `kIOHIDDigitizerEventFromEdgeTip` |
| Why | UIKit requires the edge bit for the home gesture | otherwise the swipe is just a drag |

0071 added the edge-tip bit. The autoswipe starts at y=2390 so the first down
already carries the edge flag; all three attempts logged `edge`.

## Boot args

| Argument | Meaning | 0084 value |
|---|---|---|
| `sweetautoswipe` | first delay in ms; 0 disables | `90000` |
| `sweetautoswiperetry` | retry interval in ms (min 1000) | `15000` |
| `sweetautoswipeattempts` | attempts (max 8) | `3` |

## Profiles

| Variant | Y path | Step | Note |
|---|---|---|---|
| 0 | 2390→2100→1800→1500→1200 | 50 ms | short |
| 1 | 2390→…→380 | 40 ms | long |
| 2 | 2390→2150→…→900 | 60 ms, x=520 | slow |

Three variants were tried in one boot to avoid a tuning loop. The third attempt
unlocked on the author's device; the log has no success flag, so the exact
winning attempt is observational.

## Evidence

- `SWEET HID: autoswipe armed in 90000 ms attempts=3 retry=15000 ms`.
- Attempt 1: variant 0, down `540,2390 edge`, up `540,1200 edge`.
- Attempt 2: variant 1, down/up edge.
- Attempt 3: variant 2, down `520,2390 edge`, up `520,900 edge`;
  `autoswipe done after 3 attempts`.
- No lsd/SpringBoard/backboardd crash; `driverkitd` and `PosterBoard` crashed
  (PosterBoard in first-time poster config, not HID).
- boot22 (first boot after the patch): 58 s; boot23 (warm reboot): 22 s to
  lock screen, unlocked to home, animations normal.

## Real touch next

The Goodix GT9916R is on SPI3 (`0x11013000`, IRQ GPIO14, reset GPIO15, DVDD
GPIO110). The TM/GVO firmware variant must be confirmed from Android before
writing the driver. Feed coordinates into the existing `touch()`; do not
rebuild the HID layer.

## Userland constraint

The current iOS root has no `/bin/sh`, `/usr/bin/log`, `launchctl` or
`notifyutil`; `hidutil` only reads/sets HID properties. A userland injection
would need a signed/entitled arm64 binary, so the kernel timer was the smaller
path.
