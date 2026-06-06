#!/bin/bash

# 获取脚本所在目录
SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

# 设置SDK路径
export SDK_RTSMART_SRC_DIR="/home/sgf/ws/k230_pangofly/src/rtsmart"
export NNCASE_SRC_DIR="${SDK_RTSMART_SRC_DIR}/libs/nncase"
export OPENCV_SRC_DIR="${SDK_RTSMART_SRC_DIR}/libs/opencv"
export MPP_SRC_DIR="${SDK_RTSMART_SRC_DIR}/mpp"

# 设置工具链路径
TOOLCHAIN_PATH=~/.kendryte/k230_toolchains/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin
echo "工具链路径: $TOOLCHAIN_PATH"

# 检查工具链
if [ ! -f "$TOOLCHAIN_PATH/riscv64-unknown-linux-musl-gcc" ]; then
    echo "错误：工具链不存在！"
    exit 1
fi
echo "检查工具链: 找到"

# 添加工具链到PATH
export PATH=$TOOLCHAIN_PATH:$PATH

# 创建build目录
BUILD_DIR="${SCRIPTPATH}/build"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

# 运行cmake
echo "开始配置cmake..."
cd ${BUILD_DIR}
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${SCRIPTPATH}/cmake/Riscv64.cmake" \
    "${SCRIPTPATH}"

# 检查cmake是否成功
if [ $? -ne 0 ]; then
    echo "cmake配置失败!"
    exit 1
fi

# 编译
echo "开始编译..."
make -j$(nproc)

# 检查编译是否成功
if [ $? -eq 0 ]; then
    echo "编译成功!"
    make install
    echo "安装完成，可执行文件位于: ${BUILD_DIR}/bin/gesture_demo.elf"
else
    echo "编译失败!"
    exit 1
fi
