# 可复现性保证

[English](GUARANTEE.md) · **中文**

**状态 2026-10-21 —— 文档级（理论）保证。**
这是对成功路径的文档化预期，不是已经独立复现的设备保证。由于时间和精力
限制，更详细的测试保证暂缓。

## 范围

只覆盖成功路径：

```
stock LK -> XNU 8796 -> iOS 16.7.16 用户态
  -> 锁屏 -> 合成上滑 -> SpringBoard 桌面
```

失败路线是历史证据，不在保证范围内。

## 定义

同时满足以下条件时：

- 同一设备情况（ruby / MT6877，Android 12 / MIUI 13.0.4，已解锁，slot
  `_a`，分区布局见 `docs/zh/device.md`）；
- 同一 boot 镜像和 iOS root/ICL 状态（见下方 hash）；
- 启动参数为 `config/0084-boot.cmdline`；
- 按 `docs/zh/09-reproduce.md` 和 `docs/zh/05-userland.md` 的步骤操作；

则**预期**能复现成功状态。“预期”就是“理论”的部分：没有做过第二台设备或
干净室验证。

任何一项 hash 不同，本保证不适用；那是一个不同的配置。

## 成功状态 bundle

| 项 | 值 | 说明 |
|---|---|---|
| 基线 | `MakrSas/ios-topaz@5095ac1` | 上游 commit |
| Patchset | `de1824c2...` | 87 个文件；recipe 输出 |
| 0010/0012 改动 | `1edb6633...` | 小的基线补丁 |
| 原始内核 | `b38fca5f...` | 实际启动的那颗 |
| CI 重建内核 | `3ceb9b34...` | 公开前 run；未刷机 |
| boot 镜像 | `94c639d1...` | 外部；不在仓库 |
| root 镜像（A） | `e698339d...` | 外部；成功快照；用前复核 |
| ICL 原块 | `ae317804...` | 还原路径 |
| ICL 补丁块 | `4a489542...` | 绕过 |
| 启动参数 | `config/0084-boot.cmdline` | boot23 实际 cmdline |
| 成功判据 | 首次启动约 60 秒（实测 58 秒）；热重启约 22 秒；上滑到桌面；动画正常 | 作者 boot22/boot23 |

root 状态（全部为外部工件；完整镜像不在仓库）：

| 状态 | SHA-256 | 含义 |
|---|---|---|
| A 成功快照 | `e698339d...` | boot22 之后；ICL NOP + 首启状态 |
| B 首启前 | `985c30...` | pulled+seeded；ICL 原块；boot22 diff 把 B -> A |
| C clean 基础 | `34c2882d...` | mkiosroot + tweaks；还需要首启/seed |

success bundle 是 A。旧 pin 写的是 B；2026-10-22 只读 root 审计后修正。

完整 hash：

```text
patchset   de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360
0010/0012  1edb66332bb4daa8b1abcd6c4de77a0e88ed9c7e9bb4a2a7045a2696e53ab338
kernel     b38fca5f38cfb721e21228eac4dd4b355000d1b850bdf204afba9ff8b8d1afcd
ci kernel  3ceb9b3490f8c8ecaa74348c3c5cb02d7302d0ec7f87006fe8354e9d2011a831
boot       94c639d1f45197718c62bd09c32c72b39710ba23de60f4a04bba96014bce1cb6
root A     e698339d0e228609c2d98f16ae8dd44a5149f2ccc19a44b98d5d08845c53f96f
root B     985c30fcca28cb79e6ad6d30a1ff4b4b6b7306b69d6e480cd7cccf4fb5ca11e3
root C     34c2882d3b722e53c9755dad3b74284d6ebba6f00256838d407ba3694b1a456d
icl orig   ae3178046fe7cb9e08291c39dc4773429be26c35472c6fbd9135d5f2a225fd73
icl pat    4a4895422dcec766b90b65c067ecb06ef8fd3e346c03ee7e2464a85f62a73b74
```

root 和 boot 镜像**不在仓库**：它们含不可分发的 Apple/Android 材料。hash
就是契约。

设备基线 hash（GPT/boot/vbmeta）有意**不发布**，因为可能成为设备指纹；
设备情况写在 `docs/zh/device.md`。上表的工件 hash 才是契约。

## 保证

- Patchset 和 recipe：workflow 只有在 recipe 还原出 `de1824c2...`（87 个
  文件）时才编译；CI 已绿。
- 离线工具：host 检查、合成与真实 ICL 块往返。
- 成功路径的文档化步骤和范围。
- 失败路线的历史证据，明确标注不支持。

## 重建

`scripts/build-payload.sh` + `tools/image/pack_boot.py` 使用上面的输入和 seed
`0x84` 产出 `a0ce0fe1...`。重建镜像与原件**布局等价，但不是字节一致**：
原镜像用了 `os.urandom` filler。重建镜像用 `tools/image/boot_layout.py`
对比；成功镜像仍是 `94c639d1...`。

## 不保证

- 另一台设备上的独立启动。
- 精确 UI 像素、时序或应用状态。
- CI 重建内核的启动（未刷机）。
- 原始（未打补丁）ICL 块的回归。
- root/boot 工件本身（不在仓库）。
- 原 boot 镜像的字节级重建（原 filler 是随机的）。
- 任何失败路线的设备行为。

## 暂缓的验证

因时间和精力暂缓，不是因为已知失败：

- 第二台设备启动测试；
- Linux 干净室 host CI；
- 刷入并启动 CI 重建内核；
- 原始 ICL 块回归；
- 真实触摸、按键和 USB；
- 长时间稳定性；
- 设备回归测试：这次打包没有做（作者决定，2026-10-21）。

设备基线 hash（GPT、boot_a/b、vbmeta）有意不发布；设备情况写在
`docs/zh/device.md`。

这些是明确的未完成项。做完之前，本文件保持**文档级/理论级保证**。

## 使用方式

1. 先把自己的设备情况和工件 hash 与上表对比。
2. 按 `docs/zh/09-reproduce.md` 和 `docs/zh/05-userland.md` 操作。
3. 任何 hash 不同就停止：那是不同配置，不是本保证覆盖的复现。
4. 如果做了独立复现，记录 hash 和结果（脱敏），并更新本文件。

## 失败路线

`patches/historical/0081-0083` 和 `docs/zh/07-experiments.md` 的失败目录是
证据。它们**不属于**成功构建，没有做设备重测，也不在保证范围内。
