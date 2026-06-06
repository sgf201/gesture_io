#!/bin/bash
# Linux 版本手势识别 demo 编译脚本

# 设置工具链路径
TOOLCHAIN_PATH=~/.kendryte/k230_toolchains/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin
export PATH=$TOOLCHAIN_PATH:$PATH

# 设置 SDK 路径（用于头文件和库）
SDK_LINUX_PATH=/home/sgf/ws/k230_linux_pangofly

# 清理旧的构建
rm -rf build_linux
mkdir build_linux
cd build_linux

# 运行 CMake
cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${TOOLCHAIN_PATH}/riscv64-unknown-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=${TOOLCHAIN_PATH}/riscv64-unknown-linux-gnu-c++ \
    -DCMAKE_FIND_ROOT_PATH=${SDK_LINUX_PATH}/output/k230_linux_defconfig/staging \
    ..

# 编译
make -j4

echo "Build complete!"
ls -la bin/