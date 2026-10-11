# K230 手势识别演示程序

## 项目概述

基于 K230 Linux SDK 开发的手势识别程序。摄像头采集图像，AI 模型识别手势（握拳、手掌、剪刀手等），识别结果通过串口输出，同时支持 HTTP 调试图片和 UDP 命令转发。

## 功能特性

- 手部检测：`hand_det.kmodel` 检测手部位置
- 关键点检测：`handkp_det.kmodel` 检测手部 21 个关键点
- 手势识别：基于关键点角度判断手势类型，稳定后串口输出 `@#gesture#@`
- 实时显示：DRM 屏幕绘制检测结果（模式1）
- HTTP 调试图片：浏览器访问 `http://<板子IP>:8080/` 获取带手框/关键点/手势的调试图
- UDP 命令转发：UDP 50000 端口收到命令后通过串口 `@#cmd#@` 转发

## 硬件要求

- K230 开发板（庐山派等，板载 2.4G WiFi 或外接 USB WiFi 模块）
- 摄像头（GC2093 等）
- 显示屏（可选，模式1 下接屏显示）

## 编译环境准备（WSL2 / Ubuntu 24.04）

### 1. 交叉编译工具链

K230 Linux SDK 官方工具链（Xuantie-900 glibc V3.0.2）：

```bash
mkdir -p ~/.kendryte/k230_toolchains
wget https://kendryte-download.canaan-creative.com/k230/downloads/dl/gcc/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2-20250410.tar.gz
tar -zxvf Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2-20250410.tar.gz -C ~/.kendryte/k230_toolchains/
```

工具链路径：`~/.kendryte/k230_toolchains/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2/bin`

### 2. K230 Linux SDK

SDK 提供 nncase、opencv、libdrm、v4l2-drm、display 等头文件和库（staging sysroot）：

```bash
git clone https://github.com/kendryte/k230_linux_sdk.git ~/ws/k230_linux_sdk
cd ~/ws/k230_linux_sdk
make CONF=k230_canmv_defconfig   # 庐山派用 k230_canmv_lckfb_defconfig
```

> 完整固件构建需要较长时间（buildroot 全量编译），构建完成后生成 staging 目录和可烧录镜像 `output/<defconfig>/images/sysimage-sdcard.img.gz`。

### 3. 编译本项目

```bash
cd ~/ws/gesture_io
bash build_linux.sh
```

可用环境变量覆盖默认路径：

```bash
TOOLCHAIN_PATH=/path/to/toolchain/bin \
SDK_LINUX_PATH=/path/to/k230_linux_sdk \
SDK_DEFCONFIG=k230_canmv_lckfb_defconfig \
bash build_linux.sh
```

编译产物：`build_linux/sq_handkp_class.elf`

## 运行

### 模式1：摄像头实时（部署模式）

```bash
./sq_handkp_class.elf hand_det.kmodel None 0.5 0.5 handkp_det.kmodel 0
```

- 摄像头连续推理，识别结果串口输出 `@#fist#@` 等
- HTTP 调试图：`http://<板子IP>:8080/`
- UDP 命令转发：`echo "led_on" | nc -u <板子IP> 50000`

### 模式2：本地图片单帧测试

```bash
./sq_handkp_class.elf hand_det.kmodel test.jpg 0.5 0.5 handkp_det.kmodel 1
```

结果保存为 `hand_keypoint_class_result.jpg`。

## 部署到开发板

板子启动后通过 WiFi 或 USB 网卡获取 IP，然后：

```bash
scp build_linux/sq_handkp_class.elf root@<板子IP>:/root/
scp kmodel/hand_det.kmodel root@<板子IP>:/root/
scp kmodel/handkp_det.kmodel root@<板子IP>:/root/
```

开机自启：把 `gesture_demo.service` 放到板子 `/etc/systemd/system/`，执行 `systemctl enable gesture_demo`。

## License

BSD License
