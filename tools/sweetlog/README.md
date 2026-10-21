# SWEETLOG decoder

`decode.py` turns the SWEETLOG ring into readable kernel log text.

The area is the last 1025 4 KiB blocks of the root partition: one 4 KiB header
followed by a 4 MiB ring. The decoder prints the version, ring size, total
bytes and flush sequence; a wrapped ring loses the oldest bytes.

```sh
python tools/sweetlog/decode.py AREA.bin [OUT.txt]
```

Pull it read-only from TWRP:

```sh
adb shell "dd if=<iosroot> bs=4096 skip=<size/4096-1025> count=1025 of=/tmp/area.bin"
adb pull /tmp/area.bin AREA.bin
```

`<size>` is the partition size in bytes (`blockdev --getsize64 <iosroot>`).
The pull is read-only; the decoder does not write to a device.

The ring is volatile: it changes every boot and can wrap. Record its SHA-256
and the decoded text SHA-256 as run evidence, not as a fixed image hash.
