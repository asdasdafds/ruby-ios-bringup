# Glossary and codenames

**English** · [中文](../zh/glossary.md)

## Codenames

| Name | Expansion | Meaning here |
|---|---|---|
| `ruby` | Redmi Note 12 Pro 5G | MT6877 / Dimensity 1080; the target device |
| `sweet` | Redmi Note 10 Pro | SM7150; the original `ios-sweet` target |
| `topaz` | Redmi Note 12 4G | SM6225; the `ios-topaz` target; the build baseline |
| `D22AP` | iPhone X product code | iOS 16.7.16 (20H392) userland |
| `v14` | upstream root image version | pulled root + seed; not a fresh IPSW root |
| `boot22` / `boot23` | author's boot numbers | lock screen / home screen milestones |

## Boot and storage

| Term | Expansion | Meaning here |
|---|---|---|
| `LK` | Little Kernel | Xiaomi bootloader; no `boot:` command |
| `AVB` / `AVB0` / `AVBf` | Android Verified Boot | footer at `0x7ffffc0`; keep it in the full image |
| `ADT` | Apple Device Tree | generated for the port; passed with the kernel |
| `DTB` / `FDT` | Device Tree Blob / Flattened DT | MTK DTB and the live FDT |
| `SWEETXNU` | loader payload magic | header at raw+0x14000 |
| `FEXTRA` | gzip extra field | pads the kernel stream to the stock size |
| `UFS` / `LUN` | Universal Flash Storage / logical unit | LUN 2 holds the GPT and user area |
| `iosroot` | iOS root partition | separate HFS+ partition in the dual-boot example |
| `userdata` | Android data partition | default upstream root location |
| `HFS+` / `F2FS` | Apple / Android filesystems | iOS root / Android data |
| `SWEETLOG` | persistent kernel log ring | 4 MiB ring at the end of the root |
| `expdb` | LK crash/log partition | 40 MiB; read with `dd` |

## Display and input

| Term | Expansion | Meaning here |
|---|---|---|
| `IOSurface` | graphics surface | iOS renders into it |
| `IOMFB` / `AppleCLCD` | IOMobileFramebuffer | copies the surface to the panel |
| `DSI` / `DSC` | display serial interface / compression | panel link |
| `HID` | human interface device | `IOHIDEventService` path |
| `SweetHID` | kernel HID service | `SweetHIDTouch` / `SweetHIDButtons` |
| `edge tip` | `kIOHIDDigitizerEventFromEdgeTip` | required for the home gesture |
| `GVO` / `TM` | Goodix firmware variants | **TM** confirmed for this unit; GVO unused |
| `SWEET GT` | XNU Goodix driver log tag | `bsd/dev/sweet_goodix_spi.c` |
| `kpd` | keypad driver | `mtk-kpd @ 0x10010000` |

## Userland

| Term | Expansion | Meaning here |
|---|---|---|
| `MG` / `MGIOKitSupport` | MobileGestalt | waits for `IONetworkController` |
| `LS` / `lsd` | LaunchServices / daemon | first-boot journal recovery |
| `ICL` | InstalledContentLibrary | contains the MG call site |
| `ICL NOP` | 4-byte bypass | `bl` → `nop` at `0x1b11b9f0c` |
| `MI*` | MobileInstallation | installation identity path |
| `AMFI` | Apple Mobile File Integrity | disabled in this bring-up |
| `SEP` | Secure Enclave | not ported |

## Status tags

| Tag | Meaning |
|---|---|
| `[measured]` | read or executed on the author's unit |
| `[DTS]` | from Xiaomi `ruby-s-oss` device tree |
| `[inferred]` | reasoned, not directly measured |
| `[historical]` | observed in an earlier boot; not re-run |
