import hashlib
import struct
import sys

PAGE_OFF = 36
KERNEL_SIZE_OFF = 8
RAMDISK_SIZE_OFF = 16
SECOND_SIZE_OFF = 24
DTB_SIZE_OFF = 1648
HEADER_SIZE = 1660


def pad(data, page):
    return data + b"\0" * ((page - len(data) % page) % page)


def main():
    stock_path = sys.argv[1]
    payload_path = sys.argv[2]
    out_path = sys.argv[3]
    stock = open(stock_path, "rb").read()
    payload = open(payload_path, "rb").read()

    if stock[:8] != b"ANDROID!":
        raise SystemExit("not an Android boot image")
    page = struct.unpack_from("<I", stock, PAGE_OFF)[0]
    kernel_size = struct.unpack_from("<I", stock, KERNEL_SIZE_OFF)[0]
    ramdisk_size = struct.unpack_from("<I", stock, RAMDISK_SIZE_OFF)[0]
    second_size = struct.unpack_from("<I", stock, SECOND_SIZE_OFF)[0]
    dtb_size = struct.unpack_from("<I", stock, DTB_SIZE_OFF)[0]

    def pages(n):
        return (n + page - 1) // page * page

    kernel_off = page
    ramdisk_off = kernel_off + pages(kernel_size)
    second_off = ramdisk_off + pages(ramdisk_size)
    dtb_off = second_off + pages(second_size)

    mtk_dtb_magic = b"\xd7\xb7\xab\x1e"
    if dtb_size and stock[dtb_off:dtb_off + 4] != mtk_dtb_magic:
        raise SystemExit(f"MTK dtb header magic not found at {dtb_off:#x}")

    ramdisk = stock[ramdisk_off:ramdisk_off + ramdisk_size]
    dtb_block = stock[dtb_off:dtb_off + 0x40 + dtb_size]

    header_page = bytearray(stock[:page])
    struct.pack_into("<I", header_page, KERNEL_SIZE_OFF, len(payload))
    digest = hashlib.sha1(payload + ramdisk + stock[second_off:second_off + second_size]).digest()
    header_page[576:608] = digest + b"\0" * 12

    image = bytes(header_page) + pad(payload, page) + pad(ramdisk, page)
    if second_size:
        image += pad(stock[second_off:second_off + second_size], page)
    image += pad(dtb_block, page)
    open(out_path, "wb").write(image)

    print(f"page={page} kernel={len(payload)} ramdisk={len(ramdisk)} dtb_block={len(dtb_block)}")
    print(f"image={len(image)} bytes")


if __name__ == "__main__":
    main()
