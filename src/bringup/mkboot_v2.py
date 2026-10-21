import hashlib
import struct
import sys

PAGE = 2048
KERNEL_ADDR = 0x40080000
RAMDISK_ADDR = 0x51100000
TAGS_ADDR = 0x47C80000
CMDLINE = b"console=tty0 mtk_printk_ctrl.disable_uart=1"


def pad(data, page=PAGE):
    return data + b"\0" * ((page - len(data) % page) % page)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "payload.bin"
    dst = sys.argv[2] if len(sys.argv) > 2 else "boot-red.img"
    kernel = open(src, "rb").read()
    ramdisk = b""

    hdr = bytearray(1660)
    hdr[0:8] = b"ANDROID!"
    struct.pack_into("<I", hdr, 8, len(kernel))
    struct.pack_into("<I", hdr, 12, KERNEL_ADDR)
    struct.pack_into("<I", hdr, 16, len(ramdisk))
    struct.pack_into("<I", hdr, 20, RAMDISK_ADDR)
    struct.pack_into("<I", hdr, 24, 0)
    struct.pack_into("<I", hdr, 28, 0)
    struct.pack_into("<I", hdr, 32, TAGS_ADDR)
    struct.pack_into("<I", hdr, 36, PAGE)
    struct.pack_into("<I", hdr, 40, 2)
    struct.pack_into("<I", hdr, 44, 0x0C000000)
    hdr[48:64] = b"ruby-test".ljust(16, b"\0")
    hdr[64:64 + len(CMDLINE)] = CMDLINE

    digest = hashlib.sha1(kernel + ramdisk).digest()
    hdr[576:608] = digest + b"\0" * 12

    image = pad(bytes(hdr)) + pad(kernel) + pad(ramdisk)
    open(dst, "wb").write(image)
    print(f"{dst}: {len(image)} bytes, kernel {len(kernel)} bytes")


if __name__ == "__main__":
    main()
