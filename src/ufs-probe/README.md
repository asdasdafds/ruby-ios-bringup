# UFS read-only probe

Freestanding AArch64 probe for the MT6877 UFS host. LK has already initialised
the controller and PHY to read `boot_a`, so the probe reuses the running host:
it dumps UFSHCI registers, takes over the transfer request list, and issues
TUR / READ CAPACITY / READ(10) on LUN 0..2. **There is no SCSI write path.**

| File | Role |
|---|---|
| `ufs.c` | UFSHCI setup, LUN scan, TUR/CAPACITY/READ(10), GPT read |
| `ruby_ufs_main.c` | probe entry point and exception handler |
| `ufs.h` | probe entry declaration |
| `console.h` | minimal console interface used by the probe |
| `build_ufs_full.py` | pack the probe shim into a boot image |

The console implementation lives in the bring-up shim and is not included in
this repository; `console.h` only declares the interface. Build the shim
against the same framebuffer layout, then pack it with:

```sh
python build_ufs_full.py --base stock.img --shim Image --out probe.img
```

Observed on the author's device: LUN 0/1 are 4 MiB raw LUNs, LUN 2 is the
256 GB user area with a GPT; all READ paths completed with `rc=0, ocs/st=0`.
The probe does not write userdata.
