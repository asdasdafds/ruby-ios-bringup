# Android su / TWRP 只读取证

目标：填 `contract.md`。不写分区、不加载自编模块、不改 GPIO/regulator。

## 哪个更彻底

| 维度 | Android + su | TWRP |
|---|---|---|
| 内核/驱动 | 量产 MIUI 内核 + vendor 模块，最真实 | TWRP 自带内核，可能同源但配置不同 |
| dmesg | 真实 Goodix probe、固件选择、SPI 参数 | TWRP 内核的 probe；不能单独证明 MIUI 的固件选择 |
| 输入映射 | `getevent -lp`、`dumpsys input`、`/proc/bus/input/devices` 完整 | 通常只有 `getevent`/sysfs；没有 Android framework |
| vendor 固件 | `/vendor/firmware`；root 也能 raw 读 | mount `/vendor`；raw 访问更方便 |
| DT | live DT 可读 | live DT 可读，但可能是 TWRP 的 DT |
| sysfs/debugfs | 最全：input、interrupts、regulator/clk/gpio（若开启） | 取决于 TWRP 内核配置，常缺 debugfs |
| 属性 | `getprop` 全套 vendor props | 无完整 property 服务 |
| 成本 | 需要 Android 能启动且 root；当前要恢复 Android | 需要占 `boot_a` 刷 TWRP |

**结论：**

- 两者都能进：**以 Android su 为主**，拿真实驱动行为、固件选择和输入映射；
  TWRP 只做 vendor/DT 的原始对照和备份。
- 只能进一个：当前机器是 iOS 状态，**TWRP 成本更低**（只换 `boot_a`），
  先拿 dmesg/DT/vendor firmware；若固件变体仍不明确，再恢复 Android 用 su
  做最终确认。
- Android su 在 SELinux 下可能读不到 debugfs；用 `su -c`，必要时只在当次
  会话 `setenforce 0`，不要改持久状态。

## 安全前提

- 当前 `boot_a` 是 0084。刷 TWRP 前备份 0084 镜像和回滚路径。
- 只读命令不要写 partition、不要 `dd` 回设备、不要改 GPIO/regulator。
- 需要 dump vendor 时写到外部存储，不入库；仓库只放脱敏结论和 hash。
- TWRP 的 dmesg 只能证明“TWRP 内核能驱动”，不能单独证明 MIUI 选的是
  哪个固件变体；最终变体以 Android su 的 dmesg 为准。

## Android su 命令

### 环境与版本

```sh
id
getprop ro.product.device
getprop ro.build.display.id
getprop | grep -iE 'touch|tp|panel|goodix|display'
uname -a
cat /proc/cmdline
```

### 驱动与固件选择

```sh
dmesg | grep -iE 'goodix|gt9916|gt9|touch|spi|firmware'
ls -l /vendor/firmware | grep -iE 'goodix|gt9|m16'
find /vendor -iname '*goodix*' -o -iname '*gt99*' 2>/dev/null
sha256sum /vendor/firmware/goodix_firmware_m16_*.bin 2>/dev/null
```

要找的证据：芯片 ID、SPI mode/speed、复位时序、加载的固件文件名（TM/GVO）、
是否有 “firmware download”/“flash” 日志。

### DT 节点

MTK 常见路径是 `/proc/device-tree`；没有就试 `/sys/firmware/devicetree/base`。

```sh
for d in /proc/device-tree /sys/firmware/devicetree/base; do
  [ -d "$d" ] && echo "$d"
done
find /proc/device-tree /sys/firmware/devicetree/base -maxdepth 4 \
  -iname '*touch*' -o -iname '*goodix*' 2>/dev/null
```

找到节点后，逐个看属性（DT 属性是二进制，用 `hexdump`）：

```sh
node=/proc/device-tree/<找到的节点>
for p in compatible reg spi-max-frequency interrupt-parent interrupts \
         reset-gpios irq-gpios dvdd-supply avdd-supply vddi-supply \
         firmware-name goodix,panel-type; do
  [ -e "$node/$p" ] && { echo "== $p"; hexdump -C "$node/$p"; }
done
```

### 输入映射

```sh
getevent -lp
cat /proc/bus/input/devices
dumpsys input | grep -n -i -A25 -B5 -E 'goodix|gt9916|touch'
```

要记录：input 设备名、event 节点、`ABS_MT_POSITION_X/Y` 的 min/max、
`ABS_MT_SLOT`/`ABS_MT_TRACKING_ID` 是否存在。

### 中断、时钟、电源

```sh
cat /proc/interrupts | grep -iE 'spi|touch|goodix|eint|11013000'
cat /sys/kernel/debug/regulator/regulator_summary 2>/dev/null | \
  grep -iE 'dvdd|avdd|vddi|touch|110'
cat /sys/kernel/debug/clk/clk_summary 2>/dev/null | \
  grep -iE 'spi|11013000'
cat /sys/kernel/debug/gpio 2>/dev/null | \
  grep -iE 'gpio(14|15|110)|touch'
```

**2026-10-22 实测：** 该内核 `# CONFIG_DEBUG_FS is not set`，`/d` 是悬空
symlink，debugfs 无法挂载；上面 `/sys/kernel/debug/...` 三条全部不可用。
替代来源：

- regulator：`/sys/class/regulator/regulator.*/{name,state,microvolts}`；
- pinctrl：live FDT 的 `pinctrl/ts_*` 节点（已解析）；
- clk：vendor 源码或 tracefs `clk` 事件；
- tracefs：`/sys/kernel/tracing/events/{spi,clk,regulator,gpio,irq}`；
  `spi` 事件只有 `bus/cs/len`，**没有 tx/rx payload**。

不要为了 debugfs 改内核；记录 `unavailable` 即可。

## tracefs SPI 长度（可选）

只抓 transfer 长度，不抓 tx/rx 内容。会临时打开 tracing，读完关闭：

```sh
TRACE=/sys/kernel/tracing
su -c "echo 0 > $TRACE/tracing_on"
su -c "echo > $TRACE/trace"
su -c "echo 16384 > $TRACE/buffer_size_kb"
su -c "echo 1 > $TRACE/events/spi/enable"
su -c "echo 1 > $TRACE/tracing_on"
# 触摸 60 秒
su -c "echo 0 > $TRACE/tracing_on"
su -c "echo 0 > $TRACE/events/spi/enable"
su -c "cat $TRACE/trace" > spi-trace.txt
```

`spi` 事件只有 `bus/cs/len`，没有 payload；能看每次 IRQ 的 transfer
长度序列。2026-10-22 的结果是主循环 34+6 字节，26/42 在双指/慢拖段出现。

## i2c-tools（2026-10-22）

- `/system/bin/i2cdetect`、`i2cget`、`i2cdump`、`i2cset` **都存在**。
- 但 `/dev/i2c-*` **不存在**；`CONFIG_I2C_CHARDEV is not set`，也没有
  `i2c-dev` 模块，所以这些工具无法访问总线。
- i2c-5 上的设备有 `0x1a`、`0x34`、`0x4e`、`0x64`；MT6360 regulator 是
  `5-0064`。不要 `mknod` 硬造节点；没有 chardev 驱动，节点也不会工作。
- 即使 chardev 可用，也不建议 `i2cdump` 全量扫描 MT6360：同一页可能有
  charger/PMIC 的 clear-on-read 寄存器；只能按源码指定地址做 `i2cget`。

## 驱动形态与调试节点（2026-10-22）

- **没有 `.ko` 可取**：`CONFIG_TOUCHSCREEN_GOODIX_BRL_9916=y`，Goodix
  驱动是内建的；`/proc/modules` 无 `gtx8_spi`/`goodix_ts`，
  `/sys/module/gtx8_spi` 不存在，`/vendor/lib/modules` 只有
  BT/Wi-Fi/GPS/MET 等模块。
- 驱动自带的 sysfs 节点在 `/sys/devices/platform/goodix_ts.0/`：
  `goodix_ts_chip_info`、`driver_info`、`irq_info`、`esd_info`、
  `report_rate`、`read_cfg`、`reg_rw`、`get_rawdata` 等。
  - `chip_info`：ROM `BERLIN`、patch `9916R`、config `6333c2a1`；
  - `driver_info`：`v1.2.2`；
  - `irq_info`：IRQ 215、enabled、trigger-count；
  - `report_rate`：**240HZ**；
  - `reg_rw` 直接 `cat` 返回 `EINVAL`，需要写入命令格式（源码待查）。
- kallsyms 可搜到 `goodix_spi_read_bra`、`goodix_spi_read`、
  `goodix_spi_write`、`goodix_ts_threadirq_func` 等，是源码/反汇编的锚点；
  地址被 kptr_restrict 清零，不能直接拿地址。
- **注意（2026-10-22）：** `goodix_ts_read_cfg` 本身成功返回了 config；
  真正触发内核 BUG 的是随后 `cat fwupdate/goodix_sysfs_fwsize`：
  `CFI failure (target: goodix_sysfs_fwsize_show)`，kernel panic 后重启。
  源码解释：`fwupdate` 是 plain kobject + `DEVICE_ATTR`，show 回调签名与
  `kobj_attr_show` 期望的 `struct kobject/attribute` 不匹配，CFI 直接 BUG
  （`goodix_brl_fwupdate.c:1165-1180`）。**不要读 `fwupdate/*`**
  （`goodix_sysfs_fwsize/result/update_en`），也不要写
  `reg_rw`/`reset`/`send_cfg`。只读已验证安全的
  `chip_info`、`driver_info`、`irq_info`、`esd_info`、`report_rate`。

## TWRP 命令

```sh
mount /vendor 2>/dev/null || mount /system/vendor 2>/dev/null
ls -l /vendor/firmware | grep -iE 'goodix|gt9|m16'
dmesg | grep -iE 'goodix|gt9916|gt9|touch|spi|firmware'
ls /sys/class/input
cat /proc/bus/input/devices
getevent -lp 2>/dev/null
find /sys/firmware/devicetree/base -maxdepth 4 \
  -iname '*touch*' -o -iname '*goodix*' 2>/dev/null
```

TWRP 里能触控只说明 TWRP 的 touch 驱动可用；记录它的 dmesg 作为对照，
不要把它的固件选择直接当成 MIUI 的结论。

## 证据优先级

1. MIUI `dmesg` 里的 Goodix probe / firmware 行；
2. `ruby-s-oss` vendor 驱动源码 + 同一设备 DT；
3. TWRP dmesg / sysfs；
4. 推断（必须标 `inferred`）。

每条结论写进 `contract.md` 的“证据”列；没有证据就写 `unknown`。
