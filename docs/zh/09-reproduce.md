# 09 — 复现

[English](../en/09-reproduce.md) · **中文**

> 成功路径范围、bundle hash 和不保证项见
> [`../../GUARANTEE.zh.md`](../../GUARANTEE.zh.md)。

内核构建可复现；boot 镜像和 iOS root 不是完全自包含：Apple 用户态不可
分发，0084 镜像用了上游 shim/loader/ADT 构建器，而它不在仓库里。这一章
说明哪些能从仓库复现，哪些要自备。

## 需要自备

- 已解锁的 `ruby`（Redmi Note 12 Pro 5G / MT6877），Android 12。
- iPhone X iOS 16.7.16（20H392）IPSW 和 iOS root 镜像。
- 已知可启动的完整 128 MiB Magisk/stock boot 镜像（恢复用）。
- `ruby` 的 A12 TWRP（离线 I/O）。
- 一个可用 macOS runner 的 GitHub 账号。

## 构建内核

`.github/workflows/xnu.yml` 运行 `ci/prepare-baseline.sh`。recipe 拉固定基线、
删除 topaz 补丁、加入本项目补丁、应用 0010/0012 小改动、统一换行；patchset
哈希不匹配就停止。

```text
base:     MakrSas/ios-topaz @ 5095ac19dbdcf37b64b81fe443f4027ad51b84bf
recipe:   ci/prepare-baseline.sh（不 vendor 上游树）
patchset: de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360
dxb:      blacktop/darwin-xnu-build @ 663906528d1cd5ea8fde7de6f16de73db9511549
runner:   macos-14, Xcode 15.0.1, macOS 13.5 xnu
config:   DEVELOPMENT, MACHINE_CONFIG=SWEET, -mcpu=cortex-a55
```

从 **Actions → xnu-0084 → Run workflow** 运行，或推送 `ci/`、`patches/`、
`config/`、workflow 的改动。产物：`xnu-kernel`、`xnu-buildtree`。

## Host pipeline

host 管线只用 <1 GiB 工件，不碰 16 GiB root。shim 和 ADT 是外部输入；
上游 payload 工具按固定基线 commit 拉取。

```sh
sh scripts/build-payload.sh --kernel <kernel> --adt <adt> \
  --cmdline-file config/0084-boot.cmdline --out <payload.bin>

python tools/image/pack_boot.py --base <magisk.img> --shim <Image> \
  --payload <payload.bin> --out <boot.img>

python tools/image/boot_layout.py --image <boot.img> \
  --config config/0084.env --payload <payload.bin> --base <magisk.img>
```

`--golden <0084.img>` 是可选的；没有 golden 镜像时，config pin 足够做外部
校验。root 单独按 [`root-recipe.md`](root-recipe.md) 准备。

或用 `scripts/test-pipeline.sh` 一次跑完。

0084 输入产出的 payload 是
`50701e819368239252527cc61e9bb67d4d7387798272d781f0a72470751de178`。
确定性打包器（seed `0x84`）产出
`a0ce0fe131c2a9b63da340824ad5c94db327c966c55b2dd5d7f518a15e5a0132`；
它与原 0084 镜像布局等价，但不是字节一致。原镜像用了 `os.urandom` filler；
重建镜像请用 `boot_layout.py`，不要用镜像 SHA。

这次打包没有做设备回归测试；进桌面是历史结果（boot23），CI 只覆盖构建。

## 打包 boot 镜像

0084 镜像用了上游 loader/ADT 构建器。仓库有早期 `src/bringup` 打包器和
离线校验器，但没有上游 loader 源码。完整重建请用基线的 `shim/` 和
`tools/`，或自备打包器。

必须满足：

- 完整 128 MiB；DTB/AVB0/AVB footer 保持 stock 偏移。
- kernel 槽精确 16,606,685 B（FEXTRA 补齐，无尾部零）。
- payload 在预期入口；保留 ARM64 Image 头。
- 刷前用 `tools/image/boot_verify.py` 校验。

## 刷写

刷写把整个 128 MiB 镜像写进 `boot_a`，不改 root 分区。保留 Magisk/stock
和 TWRP 镜像；b 槽不可用。

```sh
adb reboot bootloader
fastboot devices
fastboot flash boot_a <boot.img>
fastboot --set-active=a
fastboot reboot
```

## 启动参数

`config/0084-boot.cmdline` 是 boot23 的实际 cmdline。关键参数：

| 参数 | 作用 |
|---|---|
| `cpus=6` | 6 个 A55 |
| `ufsroot=iosroot`、`ufslun=2`、`ufsrw=1`、`rootrw=1` | root 设备 |
| `sweetfb_noconsole=1`、`sweetfb_hz=30` | 显示/调试 |
| `sweetautoswipe=90000`、`sweetautoswiperetry=15000`、`sweetautoswipeattempts=3` | 合成上滑 |

## 预期现象

- 打补丁后的首次启动约 58 秒；热重启约 22 秒（boot23）。上游首启 4–5 分钟。
- 锁屏：日期从 12 月 31 日开始（无 RTC/NTP），时钟会走；有相机图标和
  Home Bar。
- 合成上滑解锁到桌面，动画正常。
- 可能有黑壁纸、`Waiting...` widget、Messages badge。
- 触摸/按键/USB/Wi-Fi/GPU/RTC 未通。

## 验证

```sh
python scripts/verify-local.py
python tools/image/boot_verify.py <boot.img> <base.img>
```

`boot_verify.py` 打印镜像 SHA、page/kernel、SWEETXNU 字段、ADT SHA 和
cmdline；可选的 base 对比检查 kernel 槽之外的尾部。

## 恢复

见 [`08-recovery.md`](08-recovery.md)。每次刷前保留完整恢复镜像；没有
recovery 分区，也不能 `fastboot boot`。
