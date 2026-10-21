# Patches

These are diffs against XNU 8796 as used by the `ios-topaz` baseline. They are
not standalone source. Apply the upstream series first, then these in numeric
order.

| Patch | State | Target | Purpose |
|---|---|---|---|
| `0080-ruby-ufs.patch` | current | `bsd/dev/sweet_ufs.c` | UFS root on LUN 2, GPT header at 512 / entries 1024, `iosroot` partition |
| `historical/0081-ruby-swap-debug.patch` | historical | `iokit/Kernel/arm/SweetFramebuffer.cpp` | framebuffer VA flush / debug markers |
| `historical/0082-ruby-wifi-stub.patch` | historical | `iokit/conf/files.arm64`, `iokit/KernelConfigTables.cpp`, `iokit/Kernel/arm/SweetNetwork.cpp` | minimal `IONetworkController` stub for the MobileGestalt wait |
| `historical/0083-ruby-iokit-match-trace.patch` | historical | `iokit/Kernel/IOService.cpp` | IOKit match/notification trace |
| `0084-ruby-autoswipe.patch` | historical | `iokit/Kernel/arm/hid/SweetHID.cpp` | boot-arg synthetic swipe to unlock the lock screen (previous milestone) |
| `0085-ruby-goodix-spi.patch` | current | `bsd/dev/sweet_goodix_spi.c`, `bsd/conf/files`, `bsd/dev/sweet_ufs.c` | GT9916R BerlinD over GPIO bit-bang SPI3: MT6360 LDO3/LDO2 1.8/3.3 V, reset/IOVDD, dev_confirm/version, touch-frame poll into `sweet_hid_touch()` |

`0080` and `0085` are the changes used by the current real-touch boot. `0084`
is the previous synthetic-swipe milestone and is not required when real touch
is enabled. `0081`-`0083` are kept as evidence of the path; `0082`/`0083` were
superseded by the ICL NOP bypass. Applying a historical patch is not required
for a new build.

Check before applying:

```sh
git apply --check patches/0085-ruby-goodix-spi.patch
```

The patches modify Apple's XNU and remain under APSL 2.0; see `../NOTICE`.
