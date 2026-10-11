#!/bin/bash
# Linux 版本手势识别 demo 编译脚本

# 工具链路径（Xuantie-900 linux-6.6.0 glibc V3.0.2，K230 Linux SDK 官方工具链）
# 可用环境变量 TOOLCHAIN_PATH 覆盖
TOOLCHAIN_PATH=${TOOLCHAIN_PATH:-$HOME/.kendryte/k230_toolchains/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2/bin}
export PATH=$TOOLCHAIN_PATH:$PATH
# 供 cmake/Riscv64.cmake 读取
export GCC_PATH=$TOOLCHAIN_PATH

# K230 Linux SDK 路径（用于头文件和库，可用环境变量覆盖）
SDK_LINUX_PATH=${SDK_LINUX_PATH:-$HOME/ws/k230_linux_sdk}
# SDK 板级配置名（决定 staging 目录名，按实际编译固件时的 CONF 修改）
SDK_DEFCONFIG=${SDK_DEFCONFIG:-k230_canmv_defconfig}
SDK_STAGING=$SDK_LINUX_PATH/output/$SDK_DEFCONFIG/staging
export SYSROOT=$SDK_STAGING

if [ ! -x "$TOOLCHAIN_PATH/riscv64-unknown-linux-gnu-gcc" ]; then
    echo "错误: 未找到交叉编译器 $TOOLCHAIN_PATH/riscv64-unknown-linux-gnu-gcc"
    echo "请下载 Xuantie-900 工具链或通过 TOOLCHAIN_PATH 环境变量指定"
    exit 1
fi
if [ ! -d "$SDK_STAGING/usr/include/opencv4" ]; then
    echo "错误: 未找到 SDK staging 目录 $SDK_STAGING"
    echo "请先编译 k230_linux_sdk 固件: cd $SDK_LINUX_PATH && make CONF=$SDK_DEFCONFIG"
    exit 1
fi

# 清理旧的构建
rm -rf build_linux
mkdir build_linux
cd build_linux

# 运行 CMake
cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/Riscv64.cmake \
    ..

# 编译
make -j4

echo "Build complete!"
ls -la bin/
