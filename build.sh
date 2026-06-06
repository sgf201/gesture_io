#!/bin/bash

# 工具链路径
TOOLCHAIN_PATH="/home/sgf/.kendryte/k230_toolchains/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin"

echo "工具链路径: $TOOLCHAIN_PATH"

# 检查工具链
echo "检查工具链: "
ls $TOOLCHAIN_PATH/riscv64-unknown-linux-musl-gcc 2>/dev/null && echo "找到" || echo "未找到"

# 导出工具链到PATH
export PATH=$TOOLCHAIN_PATH:$PATH

# 创建build目录
rm -rf build
mkdir -p build
cd build

# 运行CMake
echo "开始配置cmake..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Riscv64.cmake || exit 1

# 编译
echo "开始编译..."
make -j$(nproc) || exit 1

echo ""
echo "编译完成！可执行文件位置: build/bin/sq_handkp_class.elf"
