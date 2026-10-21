# 08 — 恢复

[English](../en/08-recovery.md) · **中文**

恢复是工程约束，不是命令清单。`ruby` 没有 recovery 分区，LK 没有 `boot:`，
b 槽也坏；`boot_a` 是唯一抓手。每次实验前保留一份已知可启动的完整 boot
镜像。

## 前提

- 已解锁，slot `_a`。
- 主机上有已知可启动的完整 128 MiB Magisk/stock 镜像。
- `ruby` 的 A12 TWRP（不是 A14，不是别的机型）。
- **不要**刷 `preloader`、`lk`、`persist`、`nvram`、`nvdata`。
- 假设 `userdata` 会丢；不可重建的数据先备份。

## 为什么恢复抓手是 `boot_a`

LK 读 `boot_a/b`，解压 kernel，加载 ramdisk/DTB，然后跳转。没有独立的
recovery 分区，LK 命令表也没有 `boot:` 可以用来 RAM 启动 recovery。把 TWRP
写进 `boot_a` 是进入恢复环境的唯一方式；把已知可启动整镜像写回去是唯一
退路。

## 恢复 Android：这条写操作在做什么

整镜像包含 stock/Magisk kernel、ramdisk、DTB 和 AVB footer。刷到 `boot_a`
并设 active slot，恢复原始启动路径；不碰 `userdata`。

```sh
adb reboot bootloader
fastboot devices
fastboot flash boot_a <magisk-full.img>
fastboot --set-active=a
fastboot reboot
```

验证结果，而不只是看刷写命令返回：

```sh
adb devices
adb shell getprop sys.boot_completed   # 1
adb shell getprop ro.boot.slot_suffix  # _a
adb shell su -c id                     # uid=0(root)
```

fastboot 报 `no link` 说明 LK 会话掉了。长按电源 10–15 秒，音量下+电源进
fastboot，立刻刷；用后置 USB 口，不要 hub。

## TWRP 离线操作

- 没有 recovery 分区，也不能 `fastboot boot`；TWRP 刷到 `boot_a`。
- TWRP 启动后 ADB 通常 20–25 秒可用。
- 16 GiB 镜像用 raw `dd` 更可靠；`fastboot flash` 的 sparse 写可能在
  `iosroot` 上失败。
- 改 F2FS 大小前先卸载 `/data` 和 `/sdcard`；用 rw 挂载 + 干净卸载清
  checkpoint。
- raw 写后 `sync`，并核对 SHA。

```sh
# TWRP 示例：把 root 镜像写到 iosroot
dd if=<root.img> of=/dev/block/by-name/iosroot bs=4M
sync
```

## 恢复 ICL 块

NOP 可逆。如果它引起问题，把原始 4K 块写回，写入前后都核对块 SHA。

```sh
dd if=<icl_block_orig.bin> of=/dev/block/by-name/iosroot \
  bs=4096 seek=2438140 count=1 conv=notrunc
sync
```

原始/补丁哈希在 `05-userland.md`。

## LK 日志

Dump `expdb`，只读特征行，不要通读全量：

| 特征 | 含义 |
|---|---|
| `jump to K64` | 镜像真的被进入 |
| `hdr_loaded` / `img_loaded` / `can't find v2 dtb` | LK 拒绝镜像 |
| `decompress kernel` | gzip 流问题 |
| `panic` | 具体 assert |
| `mt_disp_update:` / `OVL` | 实际扫描 buffer |

## 硬规矩

- 没有恢复镜像不刷实验镜像。
- 不在已挂载文件系统上用 `blkpatch`。
- 不把 `userdata` 当 iOS root 的备份区。
- 不发布设备 dump、序列号、IMEI/MAC 或 Apple 固件。
