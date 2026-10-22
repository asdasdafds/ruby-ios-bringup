# bare-metal touch probe

先于 XNU 运行，复用 `src/ufs-probe` 的 freestanding shim/console 模式，把
“电源/时钟/引脚/SPI/Goodix 协议”与“XNU/HID”分开验证。

只做 PIO 轮询：**不 DMA、不 IRQ、不启动 XNU、不写 UFS**。

## 为什么先做 probe

- UFS probe 已经证明这条模式可行：[src/ufs-probe/README.md](../../src/ufs-probe/README.md)
- XNU 侧 HID 已由 0084 证明；真正的新东西只有 SPI/Goodix。
- 每次改 XNU kext 都要闪 `boot_a`；probe 变量更少，屏幕直接打印结果。

## 2026-10-22 已确认的参数

| 项 | 值 |
|---|---|
| SPI3 | `0x11013000`，5 MHz，mode 0，CS0，8-bit |
| SPI pins | **160, 161, 162, 163** → function 2（`0xa002` 高字节 0xA0=160） |
| IRQ | pin 14，EINT14，edge falling（第一版先轮询） |
| reset | pin 15，function 0 |
| IOVDD 开关 | pin 110，高=开，低=关 |
| AVDD | MT6360 LDO2 `vtp`，3.3 V，default-on |
| IOVDD | MT6360 LDO3 `VMC`，1.8 V；Android 由触控驱动 enable |
| 固件 | TM，`fw_version equal`、`config_id equal`；未观察到烧录 |

P1 先读 LK 留下的 SPI3 clock/pinmux/GPIO 状态，再按需配置；不要假设
LK 已经配好。IOVDD 的 `VMC` 是最大不确定项：如果 XNU 启动时已被关掉，
probe 需要先通过 MT6360 LDO3 打开，或者证明 LK 保持 enable。

live FDT 已给出平台基址，不需要猜：

| 节点 | 基址 |
|---|---|
| SPI3 | `0x11013000` |
| pinctrl GPIO / EINT | `0x10005000` / `0x1000b000` |
| IOCFG banks | `0x11c20000`、`0x11c30000`、`0x11d10000`、`0x11d30000`、`0x11d40000`、`0x11e20000`、`0x11ea0000`、`0x11f30000` |
| topckgen / infracfg_ao | `0x10000000` / `0x10001000` |
| I2C5 / MT6360 | `0x11d00000` / `0x64` |

FDT 只给 base/clock ID/pin-function；clock gate bit、IOCFG offset、LDO
enable bit 仍在 `clk-mt6877`、`pinctrl-mt6877`、MT6360 regulator 驱动里。

SPI 控制器（`spi-mt65xx.c`，`mediatek,mt6765-spi`）的关键点：

- 寄存器：`CFG0 0x00`、`CFG1 0x04`、`TX_SRC 0x08`、`RX_DST 0x0c`、
  `TX_DATA 0x10`、`RX_DATA 0x14`、`CMD 0x18`、`STATUS0 0x1c`、
  `STATUS1 0x20`、`PAD_SEL 0x24`、`CFG2 0x28`。
- `mt6765_compat`：`need_pad_sel`、`must_tx`、`enhance_timing`、`dma_ext`。
  `enhance_timing` 会用 `SPI_CFG2` 和 `ADJUST_CFG0`；`must_tx` 表示读也要
  填 TX dummy。
- FIFO 只有 **32 字节**；Goodix 的 34 字节预读 >32，vendor 驱动在缓冲区
  4 字节对齐时会走 **DMA**（`mtk_spi_can_dma`）。
- 如果 probe 只用 PIO：必须把 34 字节拆成 32+2 并保持 CS，使用
  pause/resume 继续；**不能拆成两次 CS 翻转的 message**，否则 Goodix
  读命令会断。

本内核 `CONFIG_DEBUG_FS` 没开，**不要指望** `clk_summary`/
`regulator_summary`/`pinctrl` debugfs。pinctrl 用 FDT（已解析），regulator
用 `/sys/class/regulator`（已拿到 vtp/VMC），clock 只能靠 vendor 源码/
probe 读寄存器。tracefs 的 `spi` 事件只有长度，没有 tx/rx 数据。

## 阶段

### P0 — 只读寄存器快照（已构建）

源码：`touch/probe/touch_probe.c`；构建：

```sh
SHIM_DIR=<ruby-shim-dir> BASE_IMG=<base-boot.img> sh touch/probe/build.sh
```

`CLANG`/`LDLLD`/`OBJCOPY` 默认取 PATH 中的工具；需要时用同名环境变量覆盖。

输出：`<workdir>/touch-p0.img`
（128 MiB），SHA-256 `b5772eeff4367bdfbbc71b1cf225e01b63222fab2c383a78bb4804cdb0b5d21c`。
base 是 `magisk_patched_full.img`，SHA-256
`9f867d75ba10367394ded7842e99220c3b4265ef78ed4d3ab1aa2a0a2932a5c8`。

P0 只读：

- SPI3：CFG0/CFG1、CMD/STATUS0、PAD_SEL/CFG2；
- topckgen：`CLK_CFG_6`、`CLK_CFG_UPDATE`；
- infracfg：IFRAO2 gate set/status；
- pinctrl：`0x10005440`、`0x10005310`、`0x100053d0`；
- GPIO：dir/do/di 的 `0x10005000/0x10005030`、`0x10005100/0x10005130`、`0x10005200`。

**2026-10-22 live P0：**

| 项 | 值 | 判读 |
|---|---|---|
| SPI3 CFG0/CFG1 | `0/0` | 控制器未初始化 |
| SPI3 CMD/ST0 | `0/0` | 未启动 |
| SPI3 PAD/CFG2 | `0/0` | 未配置 |
| TOP CFG6/UPDATE | `0x01010100/0` | mux field [9:8]=1，已是 `mainpll_d5_d4` |
| IFRAO set/sta | `0/0` | **gate 已 ungated/开**（该 CG 是 active-low：STA=0 才开） |
| PINMUX 5440 | `0x01100000` | 低 16 位为 0，SPI3 pins 还是 GPIO |
| PINMUX 5310 | `0x00220000` | pin14/15 字段 [31:24]=0 |
| PINMUX 53d0 | `0x00001111` | pin110 字段 [27:24]=0 |
| GPIO dir0/dir30 | `0xC0208408/0x0040F000` | reset15 输出、IOVDD110 输出 |
| GPIO do00/do30 | `0x80000600/0x00401000` | reset15 高；**IOVDD110 bit14=0，开关关** |
| GPIO di00 | `0xBC875FA9` | IRQ14 输入，当前为高 |

结论：LK 只留了 **top mux=1、reset 高、IRQ 输入**；**IOVDD pin 110
开关是关的**，SPI3 pinmux/gate/控制器也要由 P1 配置。P1/P1c 都没有写
`0x10005130`，所以芯片一直没拿到 IOVDD。

P0 **不写任何 MMIO**，只打印 LK 退出状态。P0 镜像会占 `boot_a`；
刷之前需要明确确认，跑完恢复 Android/Magisk 或 0084。

### P1 — 电源、复位、确认

1. 记录 LK 退出时 SPI3 clock/pinmux/CS、GPIO14/15/110 的现状；
2. pinmux/GPIO：
   - SPI3：`0x10005440` bits [15:0] = `0x2222`（pin 160-163 = function 2）；
   - pin 14/15：`0x10005310` bits [31:24] = 0（GPIO）；
   - pin 110：`0x100053d0` bits [27:24] = 0（GPIO）；
   - reset 15：DIR `0x10005000` bit15=1，DO `0x10005100` bit15；
   - IRQ 14：DIR `0x10005000` bit14=0，DI `0x10005200` bit14；
   - IOVDD 110：DIR `0x10005030` bit14=1，DO `0x10005130` bit14=1；
3. SPI3 clock：**CLR `0x100010a8` bit10=1 开钟**（SET `0xa4` 是关钟；
   STA=1 表示 gated）；mux `0x10000070` bits [9:8]，update
   `0x10000004` bit25；先读状态，divider 用保守值；
4. 上电（`brl_power_on`）：pin 110 高 → 3 ms → enable VMC 1.8 V →
   30 ms → enable vtp 3.3 V → 15 ms → reset 15 高 → 100 ms；
5. `dev_confirm`：向 `0x10000` 写 8×`0xAA`，回读 8 字节比较（3 次，
   间隔 5 ms）；
6. 可选读 `0x10014` 28 字节，校验 U8 LE；期望 ROM `BERLIN`、PID `9916R`。

通过闸门：`dev_confirm` 回读 8×`0xAA`，版本 checksum 通过，多次复位稳定。

P1 镜像已构建并运行过：
`<workdir>/touch-p1.img`，
SHA-256 `0e21b6be8302456b5a1a202dde81a285251b44346a425689c761884ab5afd5dd`。
P1 只写 SPI3 pinmux、gate、reset 脉冲和 SPI3 PIO；然后做 8 字节
`dev_confirm` 和 `0x10014` 版本前 8 字节读取。它不写 UFS/PMIC。

**P1 live（2026-10-22，含错误）：** `PRE 5440=0x01100000 sta=0`；
`POST 5440=0x01102222 sta=0x00000400`。pinmux 写入成功，但 `0xa4` 是
**SET=关钟**，所以 P1 实际把 SPI3 时钟关掉了；`SPI`/`CONF`/`VER` 全 0
是预期。

P1b 已构建并运行：
`<workdir>/touch-p1b.img`，
SHA-256 `1b65fce65457a5ddee298b66afdcdcd6795f506e69223a92ff7027a55206a4ba`。
P1b 把 mux 切到 parent 0，但继承 P1 的 SET 写法，gate 仍是关。

**P1b live（2026-10-22）：** `MUX cfg6=0x01010000`（parent 0）、
`POST 5440=0x01102222`、`sta=0x00000400`（gated）；SPI3 仍全 0。
由于 gate 被关，**不能**据此判断 mux/power。

P1c 已构建并运行：
`<workdir>/touch-p1c.img`，
SHA-256 `9c6354be1c6e9a6751466e124b724a6662cdda32ac93b7935f595081b8ead27e`。
P1c 改为 **CLR `0x100010a8` bit10 开钟**，mux 用 parent 0。

**P1c live（2026-10-22）：** `PRE sta=0`、`MUX cfg6=0x01010000`、
`POST sta=0`（ungated）；`SPI cfg0=003F003F`、`cfg2=001F001F`、
`cmd=00033000`；传输 `st=1`。但 `CONF R` 和 `VER` 数据全 0、
`CONF ok=0`。

判断：active-low gate 修复成功，SPI3 控制器和总线已经工作；Goodix
没有响应。**P1c 没有写 pin110 的 DO**；P0 的 `do30=0x00401000`
bit14=0 说明 IOVDD 开关一直是关的，这是首要嫌疑。`SPI_CFG0=003F003F`
是 CS hold/setup timing，不是 CPOL/CPHA；Mode 由 `SPI_CMD` bit8/9
决定，`CMD=00033000` 的 bit8/9=0，仍是 Mode 0。

P1d 已构建并运行：
`<workdir>/touch-p1d.img`，
SHA-256 `e45d4d943220d3e8f7a5808af2f4afe2e518638b2835dd4f6c815039aeb3aaa8`。
P1d 只加 IOVDD pin110 的 DIR/DO bit14=1，其余保持 P1c。

**P1d live（2026-10-22）：** `IOVDD dir=0040F000 do=00405000`（bit14 已置位）、
`POST sta=0`、SPI3 正常、`st=1`；但 `CONF`/`VER` 仍全 0。
所以 IOVDD 开关不是唯一缺口；VMC 是首要嫌疑，其次 CS/MISO。

**P1d 热重启（2026-10-22）：** Android 中 `VMC=enabled 1.8V`，
`adb reboot bootloader` 后刷 P1d；结果完全相同：IOVDD/SPI 正常，
`CONF`/`VER` 全 0。LK 可能重关 VMC，所以仍不能证明 VMC 无关。
必须直接量 VMC 或做 P1e I2C5 读/写 MT6360 LDO3；不要再靠热重启猜。

**P1e 计划（RMW + 复位 + 握手）：**

1. 读 MT6360（I2C5 `0x64`）reg `0x05` 旧值；读失败就中止，不写；
2. 若 bit6=0，写 `old | 0x40`；回读并打印新值；
3. 读 reg `0x11`（vtp/LDO2）确认 bit6；必要时同样 RMW；
4. 保持 pin110 IOVDD 高，延时 30 ms；
5. reset15 拉低 10 ms → 拉高 100 ms；
6. 跑 dev_confirm / VER。

**I2C5 实现注意：** live 节点 `mediatek,i2c`，DMA 区 `0x10220780`，
DTS 没有 `mediatek,fifo_only`，`i2c-mtk.c` 默认走 APDMA；P1e 不能只写
FIFO。按 DTS 的 `scl-gpio-id=132` / `sda-gpio-id=133` 做 GPIO bit-bang
（P1e 采用）。

P1e 已构建并运行：
`<workdir>/touch-p1e.img`，
SHA-256 `91dce77af9501f1666bd345a4c610a0107ef228e27825f259c51898e2b2b392e`。

安全边界：

- 先读 MT6360 LDO3 reg `0x05`；I2C NACK/超时就 `abort`，**不写 PMIC**；
- 只有 old bit6=0 才写 `old | 0x40`，随后回读打印；
- LDO2 reg `0x11` 只读/必要时同样 RMW；VTP 默认-on；
- 保持 IOVDD pin110 高 → 30 ms → reset 低 10 ms/高 100 ms；
- 最后跑 `dev_confirm`/VER。
- 只写 MT6360 LDO 的 bit6，不写 UFS，不碰其他 PMIC 位。

**P1e live（2026-10-22）：** I2C 空闲 `scl=1/sda=1`；LDO3
`old=0x80/new=0x80` bit6=0，LDO2 同样 0x80；**写 bit6 不生效**。
源码确认 MT6360 LDO 用 **CRC8 block 协议**，P1e 的普通单字节读写是错的，
0x80 不可信。

**P1f（已构建并运行）：** `touch-p1f.img`，SHA-256
`fe100afea94abf5b4cd15dc57fb517152076d3dbbd1086455de80366d56856c7`。
按 MT6360 协议：

- 写：command = `reg & 0x3f`，数据 `[value, crc8, dummy0]`，
  CRC8 覆盖 `[write_addr, reg, value]`；
- 读：读回 `[value, crc8]`，CRC8 覆盖 `[read_addr, reg, value]`；
- CRC 不匹配即 abort，不写 PMIC；只有 CRC 有效且 bit6=0 才 RMW。

**P1f live（2026-10-22）：** LDO3 `old=0x80 → new=0xC4` bit6=1；
LDO2 `old=0x80 → new=0xC4` bit6=1；CRC 协议有效，VMC/VTP 已 enable。
IOVDD/SPI 正常，但 `CONF`/`VER` 仍全 0。电源已排除；下一步查
CS/MISO/SPI 传输/复位，建议 GPIO bit-bang SPI（绕开控制器/pad-select）。

P1g 已构建并运行：
`<workdir>/touch-p1g.img`，
SHA-256 `e93b74fcb0b369b7df72d710d83c0a394b0df56c03815455f230bd476137e4e0`。
在 P1f 基础上保留 CRC8 RMW，给 VMC/VTP 30ms；IOVDD 高、
reset 低10/高100；把 SPI3 pins 160-163 切成 GPIO：

- 160=MO，161=MI，162=CSB，163=CLK（bank 5，`pinctrl-mtk-mt6877.h` 已核对）；
- mode `0x10005440` bits[15:0]=0；DIR `0x10005050` bits0-3=`1101`
  （MO/CS/CLK 输出、MI 输入）；DO `0x10005150` 空闲 CS 高、CLK 低；
- 按 Mode 0 直接 bit-bang：先 ~250 kHz，失败再 ~50 kHz；
- `dev_confirm` 写 `0x10000` 8×0xAA 后读回，最多 2 轮；
- 版本读 `0x10014` **36 字节**（8 前缀 + 28 结构体），校验 U8-LE；
- 额外自检：自己驱动 CS 并回读 DI bit2，确认 bank5 的 DI/IES 通路有效，
  否则 MISO 全 0 只能说明探针读不到，不能说明芯片没响应。

**P1f 控制器路径的 FIFO 疑点（源码对照）：** P1f 的 `spi_xfer()` 每次
都按 4 字节整字把数据压进 TX FIFO：13 字节的写命令压了 16 字节，控制器
只移出 13 字节，**FIFO 里剩 3 字节**；紧接着的 16 字节读命令会把
`00 00 00` 先发出去，读前缀整体错位 3 字节，芯片自然不会返回数据。
vendor 驱动的 `mtk_spi_set_cs()` 在每次 CS 释放时调用
`mtk_spi_reset()`（`SPI_CMD_RST`），会清 FIFO；P1f 没有这一步。
这解释了“写成功、读全 0”。P1g 的 bit-bang 路径没有 FIFO，可用来
先证明芯片；若 P1g 通过，P2 用控制器时必须在每次传输前/后置
`SPI_CMD_RST` 清 FIFO。

**P1g live（2026-10-22）：** `LDO3 0x80→0xC4`、`LDO2 0x80→0xC4`
（bit6=1）；`BB mode=01100000 dir50=B200038D do50=00000084`
（CS 高、CLK 低、MO 低、MI 输入）；`DI TEST cslo=0 cshi=1`
证明 bank5 的 DI/输入通路有效；`CONF ok=0 half=0`（250 kHz 和
50 kHz 都不响应）；`CONF R` 全 0；`VER` 全 0。
`VER CS ok=1` 是**全 0 恰好通过校验的假阳性**，已改成拒绝全 0。
结论：bit-bang 链路本身已验证，芯片在 SPI 上完全沉默；问题不在
控制器/FIFO/DI 通路。

**P1g 之后发现的关键证据（Android dmesg 1.395 s）：**

```
vtp: Bringing 1800000uV into 3300000-3300000uV
VMC: Bringing 3000000uV into 1800000-1800000uV
```

说明 LK/PMIC 留下的 **LDO3 VMC 默认是 3.0 V、LDO2 vtp 默认是
1.8 V**，Android 内核启动后才改成 1.8 V/3.3 V。P1f/P1g 只写
enable 位（bit6），没写 VOUT 寄存器，因此芯片实际拿到的是
**IOVDD 3.0 V、AVDD 1.8 V**——AVDD 严重欠压，芯片不会启动。
这也解释了为什么 warm reboot 也没用：PMIC 复位后 VOUT 回到默认，
enable 状态也回到 0（P1g 读到的 `old=0x80` 正是默认）。

**P1h（已刷入并运行，随后 panic）：** `touch-p1h.img`，SHA-256
`b4663587882cb225619985bcf2257fd780b24daad1c519ea1af5762fffc93a9b`。
在 P1g 基础上：

1. 读 LDO3 EN `0x05`、VOUT `0x09`、LDO2 EN `0x11`、VOUT `0x15`，
   全部 CRC8 校验，失败即 abort；
2. 先清两个 enable 位，断电 20 ms，制造一次真正的 power cycle；
3. VMC 写 `0x09=0x3a`（1.8 V，selector 0x3a..0x40），
   vtp 写 `0x15=0xd0`（3.3 V，selector 0xd0..0xd9），回读打印；
4. IOVDD pin110 高 → 3 ms → 使能 LDO3 → 30 ms → 使能 LDO2 → 15 ms；
5. 复位低 10 ms/高 100 ms，bit-bang `dev_confirm`、版本读；
6. 追加 26 字节触摸帧预读（`0x10308`）：校验 head、打印 event/
   touch_num/point0 raw x/y/w，最后写 0 清 sync。

**P1h 判读：** `LDO3 en=0x80 vout=0xAA`（3.0 V）、
`LDO2 en=0x80 vout=0x3A`（1.8 V）就坐实默认电压；`vout new=3A/D0`
且 `CONF ok=1`、`ROM=BERLIN` 即闭环。若电压已对仍沉默，再查
reset 极性/芯片是否被 3.0 V IOVDD 损伤。

**P1h live（2026-10-22，panic）：** 黄色正常文字打印到 `CONF` 之后，
在算 `VER CS` 时蓝屏：`ESR=0x96000021`（alignment fault）、
`FAR=0x4008C792`。反汇编定位到 `ver_ok()` 被 LLVM SLP 向量化，
生成 `ldur s0, [x0, #2]`——对 `rx+8+2` 的 4 字节非对齐 load。
probe 运行时 MMU 关闭，内存按 Device 语义处理，非对齐宽访问直接
fault。P1g 只是碰巧没触发这条优化。

**修复（P1h2）：** build.sh 加 `-fno-vectorize -fno-slp-vectorize`；
`ver_ok`/`head_ok`/`be32`/`crc8_msb` 的字节缓冲读取改为 volatile，
强制逐字节访问。已复查反汇编：没有非对齐 SIMD load，
`stur q0,[x29,#-64]` 等均 16 字节对齐。

P1h2 已构建并运行：`touch-p1h2.img`，SHA-256
`582fd761c8f6867b7ccb781c650aada8d433dff3cfeb6ee0e32669d36a8cae8e`。
逻辑与 P1h 相同，仅修编译产物。

**P1h2 live（2026-10-22）——成功：**

- `LDO3 en=0x80 vout=0xB0`（3.0 V）、`LDO2 en=0x80 vout=0x40`（1.8 V），
  坐实 LK/PMIC 默认电压；
- `LDO3 vout 0xB0→0x3A`（1.8 V）、`LDO2 0x40→0xD0`（3.3 V）；
- `CONF ok=1 half=2`（~250 kHz 第一轮通过）、
  `CONF R 0=AAAAAAAA 1=AAAAAAAA`；
- `VER CS ok=1 sum=0x04D2`、`ROM=BERLIN`、`PID=9916R`、
  `SENSOR id=0xFF vid=00800264`，与 Android dmesg 完全一致；
- `FRAME head_ok=1 ev=0x80 num=0`：`0x80` 是
  `GOODIX_TOUCH_EVENT`，芯片已在正常出帧。

结论：芯片、SPI bit-bang、复位、固件全部正常；此前唯一缺口就是
VMC/VTP 的 VOUT 寄存器。ESD/IRQ 还不参与，P2 用轮询即可。

**热重启验证（2026-10-22）：** Android 启动后确认
`regulator.61 (VMC) state=enabled 1.8V`，随后 `adb reboot bootloader`、
刷回 P1c。结果与上次完全相同：SPI3 正常、`CONF`/`VER` 全 0。
热重启**没有保住** VMC（或仍有 CS/MISO 问题），因此这个负结果
不能证明 VMC 无关；需要 P1d 直接读 MT6360 LDO3 才能定论。

BerlinD **没有** SPI mode 切换/锁 CPU/时钟/D12/软复位序列；失败时先区分：
无时钟、pinmux 不对、VMC/AVDD 没电、reset 极性错、SPI mode/speed 错、CS 不对。

### P2 — 一帧真实上报

P2 probe 已构建并运行（通过）：`touch-p2.img`，SHA-256
`3d18d7a7bd1a8b1541d8233a276c60ba3635dc830ff90532636d6ed2d3c086ec`。
保持 P1h2 的 LDO 1.8 V/3.3 V + bit-bang 上电，然后循环 **30 秒**
轮询 `0x10308` 的 34 字节：

- 拒绝全 0 head；head 校验通过才计数；
- `touch_num = data[2] & 0x0f`；1–2 点校验 point checksum，
  打印 raw x/y/w；
- 每次有效读取后写 0 清 sync；
- 每 250 ms 最多打印一行 `TCH`，每 2 s 一行 `POLL` 心跳；
- 结束打印 `frames/heads/touch/pts/min/max/last`。

刷后需要**在 30 秒窗口内用手指在屏幕上划动或点按**。

**P2 live（2026-10-22）——通过：** 电源/版本/`CONF` 与 P1h2 相同；
`TCH` 行全部 `n=1` 且 point checksum 有效，raw x 约
`0x0A34–0x2515`（2612–9493）、y 约 `0x2EEC–0x4F24`（12012–20260），
都在 10800×24000 内；`POLL frames=0xCE` 出现在 2 s 心跳，
约 80–100 Hz 轮询。方向测试（上/下/上）因采样稀疏不能单独定论；
Android `dumpsys input` 的 Touch Mapper 显示 `YScale=0.100`、
无 Affine 翻转，因此 P3 默认 `raw/10`、不翻转，XNU 实测若上下颠倒
只改一行。

1. 读 `0x10308` **26 字节**（线上 34）：8 字节 head + 2 个 8 字节 point
   + 2 字节 checksum；
2. 校验 head：前 6 字节和的低 16 位等于 `buf[6..7]`；
3. `touch_num = buf[2] & 0x0F`；若 `>2`，从 `0x10308+26` 追加读
   `(touch_num-2)*8 + 10` 字节（touch_num=3/4/5 → 线上 26/34/42）；
4. 校验 touch data：从 `buf[8]` 起 `touch_num*8+2` 字节，U8 LE；
5. 解析 8 字节 point：`[0]` 高 nibble id，`[2..3]` x LE16，`[4..5]` y，
   `[6..7]` w；
6. 处理完向 `0x10308` 写 1 字节 `0x00` 清 sync（线上 6）。

通过闸门：手指按下/抬起时 raw x/y 变化，方向和量程与 `10800×24000`
对得上，每次 checksum 都通过，sync 清零后下一帧能继续来。

不需要 host 下载固件：版本/config 相等时 vendor 驱动直接返回，不进入
flash。ESD tick（每 3 秒读 `0x10170`、必要时写 `0xAA`）第一版可以不做。

### P3 — 交给 XNU（已完成）

**P3 live（2026-10-22）：** 补丁 `0085-ruby-goodix-spi.patch` 落地
`bsd/dev/sweet_goodix_spi.c`：内核线程 + `ml_io_map`，复用本目录验证过的
LDO/复位/bit-bang SPI/帧解析，调用 `sweet_hid_touch(id, down, raw/10, raw/10)`。
实机 sweetlog：`BERLIN/9916R ready, 1080x2400`；206 down / 206 up、
40 条 finger-1、84 条 edge；真实底边上滑解锁。两指同时按下时
`SweetHIDTouch` 会把两个 finger 拼进一个 collection；用户态 pinch 未单独
验证。没有使用 `IOTimerEventSource`，
而是独立内核线程轮询（约 100 Hz），避免和 HID workloop 耦合。

后续优化：改用 SPI3 控制器（先修 FIFO 残留/DMA）、EINT14 中断、ESD tick、
>2 指。当前版本无 IRQ/ESD、最多两指。

## 打包与安全

- 复用 `src/bringup/build_shim_full.py`，或后续新建 `build_touch_full.py`；
- probe 镜像会占 `boot_a`；先备份 0084 和回滚镜像；
- console 用可见 LK 层 `0x7cdfc000`、stride 4352；
- 自旋/异常循环继续喂 WDT `0x10007008 = 1`；
- 不写 UFS、不改 GPT、不碰 `userdata`/`iosroot`。

## 源码已对照，probe 只验证平台层

Goodix 协议层已经由 `ruby-s-oss@914ba84` 的
`drivers/input/touchscreen/goodix_9916r/` 闭合：上电/复位、SPI 前缀、
26/34/42 字节、checksum、point 布局、sync 清零都有源码依据。

当初的三个平台未知（SPI3 clock gate/频率、pinmux、XNU 启动时 VMC/VTP
状态）已全部打穿；根因是 LDO VOUT 仍为 3.0 V/1.8 V。剩余工作只在性能/
中断/多指/ESD，不再是“能不能通”。
