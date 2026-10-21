# ruby-ios-bringup

**English** · [中文](README.zh.md)

**XNU 8796 + iPhone X iOS 16.7.16 userland on a MediaTek MT6877 phone.**
A research bring-up, packaged as an independent, sanitized delta.

> **Status 2026-10-21:** the first boot reaches the iOS lock screen in about
> 60 s (58 s measured); a warm reboot reaches it in about 22 s. A kernel-side
> synthetic swipe unlocks to the SpringBoard home screen; animations run.
> Real touch, buttons, USB, Wi-Fi, GPU and RTC are not implemented.
>
> **Device regression testing:** none was run for this packaging. The
> home-screen result is historical (boot23); CI covers the kernel build only.
> See [`GUARANTEE.md`](GUARANTEE.md).

This repository contains **no Apple firmware, iOS root images, device dumps or
diff data**. The kernel is built from a pinned upstream baseline plus this
project's delta; the workflow verifies the applied tree before building.

## At a glance

| Area | State |
|---|---|
| Boot | stock LK → 128 MiB boot image → XNU 8796 → HFS+ root |
| CPU | 6× Cortex-A55, 16K pages, GICv3, timers |
| Display | IOSurface/IOMFB → LK framebuffer, 1080×2400 |
| Userland | launchd → lsd → backboardd → SpringBoard |
| Input | synthetic HID swipe only; real Goodix not ported |
| Storage | UFS LUN 2, read/write HFS+ on `iosroot` |
| Not yet | touch/keys, USB, Wi-Fi, GPU, RTC |
| Safety | AMFI/sandbox/code signing off; data may be lost |

## Why this milestone matters

The hard walls were not "boot XNU" but three silent failures: the 128 MiB image
layout LK accepts, the display path that actually reaches the panel, and a
LaunchServices first-boot deadlock that looked like a kernel bug but was a
userland `MobileGestalt` consumer. The repository records the exact bytes, the
evidence and the wrong turns.

## Repository map

| Path | Contents |
|---|---|
| `README.md`, `STATUS.md`, `SCOPE.md`, `SAFETY.md` | entry points and boundaries |
| `ACKNOWLEDGEMENTS.md`, `LEGAL.md` | credits and legal risk notice |
| `docs/en/`, `docs/zh/` | English and Chinese chapters |
| `patches/` | own XNU delta: 0080 UFS, 0084 autoswipe; historical 0081-0083 |
| `ci/` | baseline recipe, canonical patchset hash and provenance |
| `config/` | 0084 build pins and boot cmdline |
| `.github/workflows/xnu.yml` | reproduces the bootable 0084 kernel |
| `src/bringup/` | early boot payloads, shim, image packers |
| `src/ufs-probe/` | read-only MT6877 UFS probe |
| `tools/image/` | ICL patch, dyld scanner, boot verifier |
| `tools/twrp/` | validated block diff/patch, TWRP-only |
| `scripts/` | baseline fetch, host verification |
| `media/` | reserved; no photos/videos yet |

## Build and verify

```sh
# host checks: no device, no image writes
python scripts/verify-local.py
```

The kernel workflow uses:

```text
base:     MakrSas/ios-topaz @ 5095ac19dbdcf37b64b81fe443f4027ad51b84bf
recipe:   ci/prepare-baseline.sh (the upstream tree is not vendored)
patchset: de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360
runner:   macos-14 / Xcode 15.0.1 / macOS 13.5 xnu
config:   DEVELOPMENT, MACHINE_CONFIG=SWEET, -mcpu=cortex-a55
```

The recipe removes the topaz patches, adds this project's patches, applies the
small 0010/0012 change, normalizes line endings and refuses to build unless the
canonical patchset hash matches. The boot image itself is **not** included;
`tools/image/` provides the offline verifier and patch tools.

Host pipeline (no device, no root): `scripts/test-pipeline.sh` with a base boot
image, kernel, ADT, shim and the golden 0084 image. It builds the payload,
packs a deterministic image and checks the layout; see
`docs/en/09-reproduce.md`.

## Device situation

| Item | Value |
|---|---|
| Device | Redmi Note 12 Pro 5G, `ruby` / MT6877, 8 GB + 256 GB |
| System | Android 12 / MIUI 13.0.4, unlocked, Magisk, slot `_a` |
| Layout | `userdata` 208.4 GiB + `iosroot` 17 GiB; `boot_a` 128 MiB; `super` ~9.1 GB |
| Current state | TWRP in `boot_a`; Android not restored; `iosroot` holds the ICL NOP |
| Success artifacts | 0084 image, kernel, root image, ICL blocks, boot cmdline |
| Must match | firmware/GPT, root hash, boot image, boot args, panel/display |
| Not needed | touch firmware, Wi-Fi, USB, GPU, RTC |

Device-baseline hashes (GPT/boot/vbmeta) are intentionally **not** published;
they can fingerprint the unit. Artifact hashes are in `config/0084.env`. See
[`docs/en/device.md`](docs/en/device.md) / [`docs/zh/device.md`](docs/zh/device.md).

## Scope and safety

- The default path writes an iOS root and can erase Android data. **Assume
  data loss.**
- The dual-boot layout is an appendix **method note**, not an installer.
- No Apple firmware, no activation against Apple servers, no personal data.
- Read [`SAFETY.md`](SAFETY.md) and [`SCOPE.md`](SCOPE.md) before flashing.
- Content policy: do not post or request Apple firmware, iOS root, dyld
  caches, device dumps, serials/IMEI/MAC or personal data. See the issue
  template (`.github/ISSUE_TEMPLATE/`) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Guarantee

The success path — boot to the lock screen and the synthetic swipe to the home
screen — is covered by a **documented, theoretical guarantee**: same device
baseline, same root/boot artifact hashes, same boot args, documented steps.
It is not an independently reproduced device guarantee. Detailed test
guarantees are deferred due to time and effort; failed routes are historical
evidence and are not covered. See [`GUARANTEE.md`](GUARANTEE.md).

## Docs

Start at [`docs/README.md`](docs/README.md), or go directly to the
[English chapters](docs/en/README.md) / [中文章节](docs/zh/README.md).

## Acknowledgements and legal

See [`ACKNOWLEDGEMENTS.md`](ACKNOWLEDGEMENTS.md) and [`LEGAL.md`](LEGAL.md).
Media capture rules are in [`media/CAPTURE.md`](media/CAPTURE.md).

## License

Original code and documentation are MIT-licensed. The patches modify Apple's
XNU and remain under APSL 2.0. Upstream projects and excluded material are
listed in [`NOTICE`](NOTICE).

**Upstream note:** `ios-sweet` and `ios-topaz` had no license file at the
pinned commits. This repository does not vendor their trees; it publishes a
recipe and only this project's patches. No upstream permission is claimed.
See [`LEGAL.md`](LEGAL.md).
