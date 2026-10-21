# 07 — 实验、失败与决策

[English](../en/07-experiments.md) · **中文**

这个项目有价值的不只是“最后怎么通的”，还有哪些假设被证伪、为什么。
这一章是为了不再走同样的弯路。

## 时间线

| 日期 | 阶段 | 变更 | 结果 |
|---|---|---|---|
| 10-19 | 整镜像 | stock/Magisk 128 MiB + 红屏 payload | LK 跳转，屏幕红 |
| 10-19 | 探针 2–6 | console `0x7cdfc000`、关 WDT、8 核、GIC/timer | 屏幕可见；8/8 CPU |
| 10-19 | Loader smoke | payload 放 `_end` 后，跳 `0x1_40000000` | 绿条；boot_args/MMU 路径通 |
| 10-19 | XNU | 1 核，然后 8 核；UFS 只读 | `8/8 CPUs initialized`；UFS 读通 |
| 10-19 | UFS root | LUN 2，GPT header 512/entries 1024 | `BSD root: ufsroot0` |
| 10-20 | 布局 | 缩小 userdata，建 `iosroot` | Android 数据完好 |
| 10-20 | 显示 | surface→panel，VA dcache flush | 标记上屏，UI 活 |
| 10-20 | 用户态 | pulled v14 + seed；cache/cpus/ADT 尝试 | 栈不变 |
| 10-20 | 内核 stub | 0082 `IONetworkController` | 服务发布；通知未被消费 |
| 10-20 | 超时 patch | `mov x1,#-1`→`#0` | lsd crash-loop；已回退 |
| 10-21 | Trace | 0083 `SWEET IOX:` | `sent=1`；内核清白 |
| 10-21 | ICL NOP | 4 字节 `bl`→`nop` | boot22 锁屏 58 秒 |
| 10-21 | 合成上滑 | 0084 内核定时器 | boot23 热重启从 Apple logo 起 22 秒，桌面，动画 |

## 失败目录

| 现象 | 可能原因 | 检查 | 状态 |
|---|---|---|---|
| b 槽 LK `hdr_loaded` | b 槽 header/AVB 路径坏 | 同镜像在 a 槽可用 | 用 `_a`；放弃 b |
| `fastboot boot` 失败 | LK 没有 `boot:` | LK 命令表 | 刷 `boot_a` |
| 屏幕不可见 | console 在 `0x7ba10000` | LK 扫描层是 `0x7cdfc000` | 用 `0x7cdfc000`、4352 |
| XNU 在 `initCPU` 后挂 | GIC/timer 未准备 | shim GIC、先 1 核 | 配 GIC、喂 WDT |
| UFS root 找不到 | LUN/GPT 偏移错 | LUN 2、header 512、entries 1024 | 补丁 0080 |
| HFS+ mount 失败 | `userdata` 是 F2FS | 分区类型 | 用 HFS+ `iosroot` |
| lsd 卡在 MG wait | 用户态消费者，不是内核 | `sent=1`、t5 循环 | ICL NOP |
| 超时 patch 后 lsd crash-loop | 改了调用者假设 | `CRASH lsd ns 2 code 0xa` | 回退；层次错 |
| 只加 ADT Wi-Fi 节点不够 | device-tree plane ≠ IOService | trace 仍注册 `IONetworkController` | 不够 |
| 内核 stub 发布仍卡 | 通知未被消费 | 0083 trace | ICL NOP |
| 黑壁纸 | PosterBoard 首启配置 | 崩溃栈 | 未解 |
| Widget `Waiting...` | WidgetKit/chronod/扩展 | 未解 | 未解 |
| 日期 12 月 31 日 | 无 RTC/NTP | 时钟是否在走 | 未解 |
| 原始 ICL 块未回归 | NOP 是绕过 | 还原后启动 | 未解 |

## 决策记录

**D1 — 完整 128 MiB 镜像。** 36.7 MB 镜像会留下不匹配 AVB footer，LK 在
`hdr_loaded` 失败；保持 DTB/AVB0/footer 在 stock 偏移。

**D2 — 只用 `boot_a`。** LK 没有 `boot:`，b 槽本身坏；每次实验刷 `boot_a`，
保留已知可启动整镜像回滚。

**D3 — 无损布局只做附录。** 它避免首次清 Android 数据，但改 GPT、用 raw
`dd`；不是默认路径，不自动化。

**D4 — ICL NOP 优于继续加内核 stub。** 内核送达已证明，墙在消费者；NOP 是
最小、可回滚的绕过。

**D5 — 内核合成上滑优于用户态注入。** root 没有 shell/日志工具；用户态 HID
注入要签名/entitlement；内核定时器复用现有 `touch()`。

**D6 — `cpus=6`（A55）。** A55 支持 16K 页，省掉 topaz 的 Silver 核处理；
A76 暂未使用。

**D7 — TWRP 放 `boot_a`。** 没有 recovery 分区、没有 `boot:`；离线 raw I/O
只能把 TWRP 刷进 `boot_a`。

**D8 — 仓库不放固件/diff 数据。** 镜像、iOS root、设备 dump、块 diff 不可
分发，也可能含个人数据。

## 不要重复

- 没有 `sent=1` 证据，不要怀疑内核通知。
- 不要用 semaphore timeout patch；它只是把崩溃换了个位置。
- 不要加个 ADT 节点就当服务。
- 不要把 LS 死锁归因于 CPU 数或 launchd cache。
- 不要指望升级 XNU 修 IOKit；8796 和 10063 在这些函数上一致。
- 不要假设 MG 缓存存在；它不存在。
- 不要把黑壁纸归因于面板。
- 不要用 b 槽或 `fastboot boot`。
- 不要把固件、dump、序列号或 diff 数据放进仓库。
