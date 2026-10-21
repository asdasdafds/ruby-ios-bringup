# 代号与术语表

[English](../en/glossary.md) · **中文**

## 代号

| 名称 | 全称 | 在本项目中的含义 |
|---|---|---|
| `ruby` | Redmi Note 12 Pro 5G | MT6877 / 天玑 1080；目标机 |
| `sweet` | Redmi Note 10 Pro | SM7150；`ios-sweet` 的原目标 |
| `topaz` | Redmi Note 12 4G | SM6225；`ios-topaz` 的目标，也是构建基线 |
| `D22AP` | iPhone X 产品代号 | iOS 16.7.16（20H392）用户态 |
| `v14` | 上游 root 镜像版本 | pulled root + seed，不是全新 IPSW root |
| `boot22` / `boot23` | 作者的启动编号 | 锁屏 / 桌面里程碑 |

## 启动与存储

| 术语 | 全称 | 在本项目中的含义 |
|---|---|---|
| `LK` | Little Kernel | 小米 bootloader；没有 `boot:` 命令 |
| `AVB` / `AVB0` / `AVBf` | Android Verified Boot | footer 在 `0x7ffffc0`；整镜像必须保留 |
| `ADT` | Apple Device Tree | 为移植生成，随内核传递 |
| `DTB` / `FDT` | Device Tree Blob / Flattened DT | MTK DTB 与 live FDT |
| `SWEETXNU` | loader payload magic | raw+0x14000 的头 |
| `FEXTRA` | gzip 扩展字段 | 把 kernel 流补齐到 stock 大小 |
| `UFS` / `LUN` | 通用闪存 / 逻辑单元 | LUN 2 放 GPT 和用户区 |
| `iosroot` | iOS root 分区 | 双系统示例里的独立 HFS+ 分区 |
| `userdata` | Android 数据分区 | 上游默认的 root 位置 |
| `HFS+` / `F2FS` | Apple / Android 文件系统 | iOS root / Android 数据 |
| `SWEETLOG` | 持久内核日志环 | root 末尾 4 MiB 环 |
| `expdb` | LK 崩溃/日志分区 | 40 MiB；用 `dd` 读取 |

## 显示与输入

| 术语 | 全称 | 在本项目中的含义 |
|---|---|---|
| `IOSurface` | 图形 surface | iOS 渲染目标 |
| `IOMFB` / `AppleCLCD` | IOMobileFramebuffer | 把 surface 拷到面板 |
| `DSI` / `DSC` | 显示串行接口 / 压缩 | 面板链路 |
| `HID` | 人机接口设备 | `IOHIDEventService` 路径 |
| `SweetHID` | 内核 HID 服务 | `SweetHIDTouch` / `SweetHIDButtons` |
| `edge tip` | `kIOHIDDigitizerEventFromEdgeTip` | home 手势必需位 |
| `GVO` / `TM` | Goodix 固件变体 | 需在 Android 侧确认 |
| `kpd` | 键盘驱动 | `mtk-kpd @ 0x10010000` |

## 用户态

| 术语 | 全称 | 在本项目中的含义 |
|---|---|---|
| `MG` / `MGIOKitSupport` | MobileGestalt | 等 `IONetworkController` |
| `LS` / `lsd` | LaunchServices / 守护进程 | 首启 journal recovery |
| `ICL` | InstalledContentLibrary | MG 调用点所在库 |
| `ICL NOP` | 4 字节绕过 | `0x1b11b9f0c` 处 `bl` → `nop` |
| `MI*` | MobileInstallation | 安装身份路径 |
| `AMFI` | Apple Mobile File Integrity | 本 bring-up 中关闭 |
| `SEP` | Secure Enclave | 未移植 |

## 状态标签

| 标签 | 含义 |
|---|---|
| `[实测]` | 在作者机器上读到或执行过 |
| `[DTS]` | 来自小米 `ruby-s-oss` 设备树 |
| `[推断]` | 推理结论，未直接测量 |
| `[历史]` | 早期启动观察到，未重跑 |
