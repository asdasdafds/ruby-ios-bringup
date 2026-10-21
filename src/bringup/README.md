# bring-up payloads

Early boot-chain experiments: the red-screen payload, the stage-marker shim,
and the packers that put a freestanding payload into an Android boot image's
kernel slot.

Sources only; no boot images or object files are committed.

| File | Role |
|---|---|
| `payload.S`, `payload2.S` | minimal AArch64 payloads used for the red-screen milestone |
| `shim.S`, `shim.c`, `shim.ld` | stage shim: entry, framebuffer fill, FDT parse |
| `mkboot_v2.py` | build a small Android boot image from a payload |
| `repack_stock.py` | repack a stock boot image with a payload and a new SHA1 |
| `build_full_a.py` | pack a payload into an existing boot image's kernel slot |
| `build_shim_full.py` | same, for the stage shim |

Examples:

```sh
python build_full_a.py --base stock.img --payload payload.bin --out out.img
python repack_stock.py stock.img payload.bin out.img
```

These tools write boot images. Verify the result with
`tools/image/boot_verify.py` and keep a known-good restore image before
flashing anything.
