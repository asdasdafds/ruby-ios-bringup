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
| `0084-ruby-autoswipe.patch` | current | `iokit/Kernel/arm/hid/SweetHID.cpp` | boot-arg synthetic swipe to unlock the lock screen |

`0080` and `0084` are the changes used by the current boot. `0081`-`0083` are
kept as evidence of the path; `0082`/`0083` were superseded by the ICL NOP
bypass. Applying a historical patch is not required for a new build.

Check before applying:

```sh
git apply --check patches/0084-ruby-autoswipe.patch
```

The patches modify Apple's XNU and remain under APSL 2.0; see `../NOTICE`.
