#!/bin/bash

# 使用 k230_linux_pangofly 的工具链
K230_LINUX_PATH="/home/sgf/ws/k230_linux_pangofly"
SDK_PATH="/home/sgf/ws/k230_pangofly"
PANGOFLY_PATH="${PWD}/pangofly"

# 从 buildroot 配置中获取工具链路径
LAST_CONF=$(cat ${K230_LINUX_PATH}/.last_conf | cut -d= -f2)
GCC_PATH=$(cat ${K230_LINUX_PATH}/output/${LAST_CONF}/.config | grep BR2_TOOLCHAIN_EXTERNAL_PATH | cut -d'"' -f2)/bin
SYSROOT=${K230_LINUX_PATH}/output/${LAST_CONF}/host/riscv64-buildroot-linux-gnu/sysroot

echo "工具链路径: $GCC_PATH"
echo "SYSROOT: $SYSROOT"

# 检查工具链
echo "检查工具链: "
ls $GCC_PATH/riscv64-unknown-linux-gnu-gcc 2>/dev/null && echo "找到" || echo "未找到"

# 设置环境变量
export PATH=$GCC_PATH:$PATH
export SDK_PATH
export PANGOFLY_PATH
export K230_LINUX_PATH

# 创建build目录
rm -rf build
mkdir -p build
cd build

# 运行CMake
echo "开始配置cmake..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/Riscv64.cmake \
    -DCMAKE_SYSROOT=${SYSROOT} \
    -DPANGOFLY_INCLUDE_DIR=${PANGOFLY_PATH} \
    -DPANGOFLY_LIBRARY=${PANGOFLY_PATH}/build_k230/libpangofly.so \
    -DGSL_INCLUDE_DIR=${SDK_PATH}/src/rtsmart/libs/nncase/riscv64 \
    -DK230_LINUX_INCLUDE_DIR=${K230_LINUX_PATH}/buildroot-overlay/package \
    || exit 1

# 编译
echo "开始编译..."
make -j$(nproc) || exit 1

echo ""
echo "编译完成！可执行文件位置: build/bin/"
ls -la bin/
