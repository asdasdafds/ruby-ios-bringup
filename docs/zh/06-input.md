# 06 — 输入与合成上滑

[English](../en/06-input.md) · **中文**

真实触摸驱动还没移植。0084 用内核定时器调用 `SweetHIDTouch::touch()`，
证明 HID 链路；真实驱动以后喂同一个入口即可。

## 链路

```
kernel timer -> SweetHIDTouch::touch()
  -> IOHIDEventService -> IOHIDEventSystem
  -> backboardd -> SpringBoard
```

定时器由 boot args 控制；不加 `sweetautoswipe` 完全不触发。

## 边缘位

| 项 | 值 | 说明 |
|---|---|---|
| 起点 Y | `2390` | 底部 48 px 边缘区内 |
| 区域判据 | `y > 2399 - 48` | 置 `kIOHIDDigitizerEventFromEdgeTip` |
| 原因 | UIKit 的 home 手势需要 edge 位 | 否则只是一次拖动 |

0071 加了 edge-tip 位。合成滑动从 y=2390 开始，第一次 down 就带 edge；
boot23 三次尝试的 down/up 全部是 `edge`。

## Boot args

| 参数 | 含义 | 0084 值 |
|---|---|---|
| `sweetautoswipe` | 首次延迟 ms；0 关闭 | `90000` |
| `sweetautoswiperetry` | 重试间隔 ms（最小 1000） | `15000` |
| `sweetautoswipeattempts` | 次数（最大 8） | `3` |

## 三种手势

| 变体 | Y 路径 | 步长 | 说明 |
|---|---|---|---|
| 0 | 2390→2100→1800→1500→1200 | 50 ms | 短滑 |
| 1 | 2390→…→380 | 40 ms | 长滑 |
| 2 | 2390→2150→…→900 | 60 ms，x=520 | 慢滑 |

一次启动里试三种手势，避免反复调参。作者机器上第三次解锁；日志没有成功
标志，具体哪次命中是观察结论。

## 证据

- `SWEET HID: autoswipe armed in 90000 ms attempts=3 retry=15000 ms`。
- 第 1 次：变体 0，down `540,2390 edge`，up `540,1200 edge`。
- 第 2 次：变体 1，down/up edge。
- 第 3 次：变体 2，down `520,2390 edge`，up `520,900 edge`；
  `autoswipe done after 3 attempts`。
- 没有 lsd/SpringBoard/backboardd 崩溃；`driverkitd` 和 `PosterBoard` 崩溃
  （PosterBoard 是首启 poster 配置，不是 HID）。
- boot22（打补丁后首次启动）：58 秒；boot23（热重启）：22 秒到锁屏，
  解锁到桌面，动画正常。

## 真实触摸的下一步

GT9916R 在 SPI3（`0x11013000`，IRQ GPIO14，reset GPIO15，DVDD GPIO110）。
写驱动前先在 Android 侧确认 TM/GVO 固件变体。坐标喂给现有 `touch()`，
不要重做 HID 层。

## 用户态限制

当前 iOS root 没有 `/bin/sh`、`/usr/bin/log`、`launchctl`、`notifyutil`；
`hidutil` 只能读写 HID 属性。用户态注入要签名/entitlement 的 arm64 二进制，
所以内核定时器是更小的路径。
