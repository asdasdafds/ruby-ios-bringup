# 附录 — 双系统布局（方法说明）

[English](../en/appendix-dualboot.md) · **中文**

这**不是**安装器，也**不是**默认路径。它只是“保留 Android 数据、另建
iOS root”的方法说明，不提供自动化。

## 默认路径

用上游工具把 iOS root 写进 `userdata`（或专用分区）。更简单，但可能清掉
Android 数据；按会丢数据来准备。

## 双系统示例

作者机器：`userdata` 208.4 GiB + `iosroot` 17 GiB；HFS+ root 在 `iosroot`，
`boot_a` 在 Android 和 XNU 之间切换。尺寸只是示例，不是别的固件的配方。

## 为什么不自动化

- GPT 改动和 raw `dd` 因机而异；没有备份就不可逆。
- `ruby` 没有 recovery 分区，LK 没有 `boot:`；离线操作必须把 TWRP 刷进
  `boot_a`。
- 尺寸或块数错了可能毁掉 `userdata` 或变砖。
- 它不让数据“安全”，只是避免第一次擦除。

## 步骤概要

1. 备份 GPT、`boot_a/b`、`vbmeta`、`persist`、`modemst1/2`、`fsg`、`fsc`
   和不可重建的数据。
2. 定好 `userdata` 与 `iosroot` 大小；保持对齐和 `flashinfo` 不动。
3. 改主/备 GPT；用只读 dump 校验。
4. 调整 F2FS `userdata`：先清 checkpoint（rw 挂载 + 干净卸载），再
   `resize.f2fs`。
5. 用 TWRP raw `dd` 把 HFS+ root 写到 `iosroot`；核对 SHA。
6. 用 `rd=ufsroot0 ufsroot=iosroot ufsrw=1 rootrw=1` 启动 XNU；保留已知
   可启动的 `boot_a` 镜像以便切回。
7. 布局验证前，保留原 GPT 和每个改动分区的前若干块。

## 只要 iOS

用默认路径，接受数据丢失。如果必须保留 Android 数据，双系统可行，但风险
自负。
