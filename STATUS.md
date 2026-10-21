# Status

As of **2026-10-21**. This is a snapshot of verified results, not a live device
status. No phone is attached to this repository or to its CI.

## Verified

| Item | Result | Evidence |
|---|---|---|
| Boot to lock screen | first boot ~60 s (58 s measured); warm reboot ~22 s (boot23) | author's device log; 0084 image SHA `94c639d1...` |
| Unlock to home screen | synthetic swipe; normal animations | author's device log and screen capture |
| ICL NOP | dyld `.15`, iosroot 4K block `2438140`, `02255e95 -> 1f2003d5` | original block SHA `ae317804...`; patched `4a489542...`; backup exists |
| UFS root | LUN 2, GPT header at 512 / entries 1024, `iosroot` partition | read/write HFS+ mount on the author's device |
| Display | IOSurface/IOMFB to LK framebuffer; 1080x2400 | surface markers and live UI |
| HID path | `SweetHIDTouch -> IOHIDEventService -> backboardd -> SpringBoard` | synthetic autoswipe reaches SpringBoard |
| Patch checks | 0084/0083/0081 apply to the pre-0084 build tree; 0080/0082 are already applied | host-side `git apply --check`, 2026-10-21 |
| ICL tool | original block -> patched block reproduces `4a489542...` | host-side round-trip, 2026-10-21 |
| Host pipeline | payload `50701e81...`; deterministic rebuild `a0ce0fe1...`; layout PASS vs 0084 | host-only, 2026-10-21; no root/phone |
| Host checks | strict host bundle PASS; config-mode layout PASS (11 checks); Linux host CI green before the final docs/template-only amendment | host-only, 2026-10-21 |

The ICL blocks, boot images and logs are **not** in this repository. Only their
hashes and conclusions are recorded.

## Build provenance (0084)

- Baseline: `MakrSas/ios-topaz` @ `5095ac19dbdcf37b64b81fe443f4027ad51b84bf` (fetched by
  commit; the upstream `topaz` branch has since moved to `89451b8`).
- Recipe: `ci/prepare-baseline.sh`; canonical patchset
  `de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360`
  (87 files). The upstream tree and the old full delta are not in the repo.
- 0010/0012 change: `ci/baseline-changes.patch`, SHA256 `1edb6633...`.
- Historical origin: `ruby@00d46e7` (the patchset was taken from it; the
  recipe itself does not fetch that branch).
- Build harness: `blacktop/darwin-xnu-build` @ `6639065`; macOS 13.5 / Xcode
  15.0.1 on a `macos-14` runner; `DEVELOPMENT` + `MACHINE_CONFIG=SWEET`,
  `-mcpu=cortex-a55`.
- Reference kernel SHA256 `b38fca5f...`; reference boot image SHA256
  `94c639d1...`. A rebuild may differ; the patchset hash is the gate.
- Workflow: `.github/workflows/xnu.yml`. It runs the recipe and enforces the
  patchset hash before building; see `ci/README.md`.
- CI: the kernel build and host checks were green before the final
  docs/template-only amendment; see Actions. Rebuilt kernel SHA256
  `3ceb9b34...`; reference original `b38fca5f...`; builds are not
  bit-identical. The rebuilt kernel has not been flashed.
- **Device regression tests: none were run for this packaging.**

## Reproducibility guarantee

The success path is covered by a **documented, theoretical guarantee** (same
device situation, same root/boot artifact hashes, same boot args, documented
steps). It is not an independently reproduced device guarantee. No device
regression tests were run for this packaging; detailed test guarantees are
deferred. Failed routes are historical evidence and are not covered. See
[`GUARANTEE.md`](GUARANTEE.md).

## Not working

- Real touch (Goodix GT9916R over SPI3) and hardware buttons.
- USB, Wi-Fi, Bluetooth, cellular, audio, camera, GPU, RTC.
- MobileGestalt cache is absent; the ICL NOP is a bypass, not a fix.
- The original (unpatched) ICL block has not been regression-tested after the
  first-boot state was created.
- PosterBoard/wallpaper and some WidgetKit surfaces are incomplete.

## Verification levels

- **host-verified**: commands and tools run on the author's PC against local
  files; see `VERIFICATION.md`.
- **device-verified (historical)**: observed on the author's phone during the
  bring-up. These results are not reproduced by CI and are not a guarantee for
  another device or firmware.
- **not verified**: everything else, including a from-scratch public
  reproduction on a different ruby unit.
