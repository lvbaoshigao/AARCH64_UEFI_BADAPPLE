# Bad Apple!! UEFI 播放器

Bad Apple 播放器源码、构建脚本和一份已构建的程序。
适用于AEM架构的CPU，以及对应的UEFI
相关适配工作可以使用agent推进
对于mov视频文件，项目里保存了一份，你可以在https://badapple.mov/下载它

## 目录

```
badapple/                    播放器本体
  BadApple.c                 独立 UEFI 程序（不依赖 EDK II 库）：校验、解码、双线性放大、
                             脏矩形输出、OWN/DIRECT/SYNC/GOP 四种输出模式、计时、按键、喂狗
  BaCodec.h                  "RBA1" 码流格式和自适应二进制区间解码器（编码器和播放器共用）
  ba-encode.c                宿主机编码器：灰度帧 -> 1 bit -> RBA1，编完逐帧回解核对
  BaBlob.S                   用 .incbin 把 badapple.bin 链接进 EFI（gBaData / gBaDataEnd）
  build-badapple-efi.sh      WSL 构建：编译编码器 -> ffmpeg 按档位逐级尝试 -> 链接 BadApple.efi
  README.md                  各版本的设计和真机记录（v1 到 v4）
anim/
  AnimTables.h               8x19 字形（取自 UniXNU Font.c / LaffStd，BSD），播放器的文字用它
  check-pe.py                PE 检查（AArch64、子系统 10、入口 = map 里的 efi_main）
  build-anim.py              整套菜单候选的构建入口（算 Bad Apple 字节预算、调用上面的脚本、打包）
  make-anim-payload.py       UniXNU 菜单配置和 FAT 卷（Bad Apple 是 Tools/BadApple.efi 一项）
prebuilt/                    已构建产物（384x288@30，6572 帧，逐帧核对全部一致）
  BadApple.efi               SHA256 70f6a5a3...，码流在文件偏移 24656
  badapple.bin               RBA1 码流本体（2,530,809 B，CRC32 fa137baa）
  BadApple.map / encode-result.json / SHA256SUMS.txt / video-probe.txt / video.sha256
  preview-*.pgm              编码器在 1/4、1/2、3/4 处回解出的画面
```

video.sha256 和 video-probe.txt 记录了所用视频的哈希和参数（h264 480x360@30，219.07 s）。

## 单独构建播放器

需要：clang、lld、ffmpeg，以及 EDK II BaseTools 里的 GenFw 和 GccBase.lds。默认从 `--root` 指定的 uefi-rothko checkout 中取（`Mu_Basecore/BaseTools`）。

```
bash badapple/build-badapple-efi.sh --video /path/to/badapple.mov --budget 2675420 \
     --out /abs/new/dir [--root {path} ] [--ladder "480x360@30 384x288@30 ..."]
```

`--budget` 是码流可用的字节数，脚本会选出预算内画质最高的一档。脚本按相对路径 `../anim/AnimTables.h` 引用字体，所以 badapple/ 和 anim/ 要保持同级。

build-anim.py 和 make-anim-payload.py 是放进 UniXNU 菜单用的，还依赖原仓库 agent16/anim 下的其它文件（RothkoAnim、BootEntries、RebootFastboot 等）、v4 内核输入和原厂镜像路径，只放在这里作参考，单独解压后不能直接运行。

## 运行

作为 UniXNU 菜单的 `type = "efi"` 项启动，也可以从固件 Boot Manager 启动（"Rothko Bad Apple"）。

- 按键：电源键返回；音量上暂停/继续；音量下切换输出模式。
- OWN 模式下按住音量上 3 秒会强制退出。
- 加载选项：`loop=1` 循环播放，`mode=0..3` 指定输出模式（默认 3 = OWN）。
- 没有声音：rothko 固件没有音频驱动。

