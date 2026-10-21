# Device situation

**English** · [中文](../zh/device.md)

This is a description, not a device-baseline hash manifest. Hashes of the GPT,
`boot_a/b`, `vbmeta` or partition GUIDs are intentionally **not** published:
they can fingerprint the unit. The success-contract artifact hashes are in
`config/0084.env`.

## The unit

| Item | Value |
|---|---|
| Device | Redmi Note 12 Pro 5G, `ruby` / MT6877 (Dimensity 1080) |
| Memory / storage | 8 GB + 256 GB |
| System | Android 12 / MIUI 13.0.4, unlocked bootloader, Magisk, slot `_a` |
| Partition layout | `userdata` 208.4 GiB + `iosroot` 17 GiB; `boot_a` 128 MiB; `super` ~9.1 GB |
| Current state | `boot_a` holds the 0084 XNU image; Android not restored; `iosroot` holds the ICL NOP |
| Success artifacts | 0084 boot image, kernel, root image, ICL blocks, boot cmdline |

## What must match

- firmware and GPT layout (the device must be the same `ruby` class);
- the root image hash and the 0084 boot image hash;
- the boot cmdline in `config/0084-boot.cmdline`;
- the panel/display path (1080×2400, LK scan buffer `0x7cdfc000`).

## What does not need to match

Touch firmware, Wi-Fi, USB, GPU and RTC are not part of the success path.

## Testing status

No device regression tests were run for this packaging. The home-screen result
is historical (boot23); CI builds the kernel only. See
[`../../GUARANTEE.md`](../../GUARANTEE.md) for the exact guarantee and the
deferred tests.
