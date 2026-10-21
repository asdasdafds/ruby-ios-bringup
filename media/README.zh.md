# 媒体

[English](README.md) · **中文**

0084 构建的脱敏素材。两个文件均重新编码：EXIF/GPS 等元数据已清除，视频不含音轨。

[<img src="home-screen.jpg" width="360" alt="合成上滑后的 SpringBoard 桌面">](autoswipe.mp4)

**点击图片播放 12 秒片段。** 画面为锁屏 → 内核合成上滑 → SpringBoard 桌面。
拍摄时手机被拿起，屏幕保持可见；没有脸部、序列号、IMEI 或账号信息。

## 文件

| 文件 | 内容 | 规格 | 大小 | SHA256 |
|---|---|---|---|---|
| [`home-screen.jpg`](home-screen.jpg) | SpringBoard 桌面，含 Dock；小组件仍显示 `Waiting...` | 1440×1920 JPEG | 270 KB | `825da7578a62e31ba40983c8cf85bc043f430170d81f416b7513b73a0e2e8df7` |
| [`autoswipe.mp4`](autoswipe.mp4) | 锁屏 → 合成上滑 → 桌面 | 720×1238，H.264，60 fps，12.0 秒 | 2.57 MB | `9a3daf27e7d2bd71c374446f47c9601bbe923dd91ed172ee1c945a0c39cb9bff` |

## 来源

- 2026-10-21 在 `ruby` 设备（Redmi Note 12 Pro 5G，MT6877）拍摄。
- 构建：0084 autoswipe boot 镜像
  `94c639d1f45197718c62bd09c32c72b39710ba23de60f4a04bba96014bce1cb6`；
  内核 `b38fca5f38cfb721e21228eac4dd4b355000d1b850bdf204afba9ff8b8d1afcd`。
- 片段截取自源录像 100–112 秒；源文件不在仓库中，且带 GPS 元数据。
- 完整 120 秒版本（1080×1856，60 fps，73.5 MiB）未入库；需要时作为
  release asset 发布。

拍摄规范：[CAPTURE.zh.md](CAPTURE.zh.md)。仓库不含 Apple 固件、root 镜像、
设备 dump 或 diff 数据。
