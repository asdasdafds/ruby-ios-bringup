# Media

**English** · [中文](README.zh.md)

Sanitized evidence for the 0084 build. Both assets are re-encoded: EXIF/GPS and
other metadata are stripped, and the clip has no audio track.

[<img src="home-screen.jpg" width="360" alt="SpringBoard home screen after the synthetic swipe">](autoswipe.mp4)

**Click the image to play the 12 s clip.** It shows the lock screen, the
kernel-side synthetic swipe and the resulting SpringBoard home screen. The
phone is picked up by hand while the screen stays visible; no face, serial,
IMEI or account data is shown.

## Files

| File | Content | Spec | Size | SHA256 |
|---|---|---|---|---|
| [`home-screen.jpg`](home-screen.jpg) | SpringBoard home screen, dock visible; widgets still show `Waiting...` | 1440×1920 JPEG | 270 KB | `825da7578a62e31ba40983c8cf85bc043f430170d81f416b7513b73a0e2e8df7` |
| [`autoswipe.mp4`](autoswipe.mp4) | lock screen → synthetic swipe → home screen | 720×1238, H.264, 60 fps, 12.0 s | 2.57 MB | `9a3daf27e7d2bd71c374446f47c9601bbe923dd91ed172ee1c945a0c39cb9bff` |

## Provenance

- Captured 2026-10-21 on the `ruby` device (Redmi Note 12 Pro 5G, MT6877).
- Build: 0084 autoswipe boot image
  `94c639d1f45197718c62bd09c32c72b39710ba23de60f4a04bba96014bce1cb6`;
  kernel `b38fca5f38cfb721e21228eac4dd4b355000d1b850bdf204afba9ff8b8d1afcd`.
- The clip is trimmed from 100–112 s of the source recording. The source is
  not in this repository and carried GPS metadata.
- The full 120 s recording (1080×1856, 60 fps, 73.5 MiB) is not committed;
  publish it as a release asset if needed.

Capture rules: [CAPTURE.md](CAPTURE.md). No Apple firmware, root image, device
dump or diff data is included.
