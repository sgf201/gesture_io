set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# Xuantie-900 linux-6.6.0 glibc gcc Toolchain V3.0.2（K230 Linux SDK 官方工具链）
# 可用环境变量 GCC_PATH 覆盖工具链 bin 目录
if(DEFINED ENV{GCC_PATH})
    set(K230_TOOLCHAIN_DIR "$ENV{GCC_PATH}")
else()
    set(K230_TOOLCHAIN_DIR "$ENV{HOME}/.kendryte/k230_toolchains/Xuantie-900-gcc-linux-6.6.0-glibc-x86_64-V3.0.2/bin")
endif()

# SDK staging sysroot（提供 opencv/nncase/libdrm/v4l2-drm/display 等头文件和库）
# 可用环境变量 SYSROOT 覆盖
if(DEFINED ENV{SYSROOT})
    set(CMAKE_SYSROOT "$ENV{SYSROOT}")
else()
    set(CMAKE_SYSROOT "$ENV{HOME}/ws/k230_linux_sdk/output/k230_canmv_defconfig/staging")
endif()

set(CMAKE_C_COMPILER ${K230_TOOLCHAIN_DIR}/riscv64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${K230_TOOLCHAIN_DIR}/riscv64-unknown-linux-gnu-g++)

# K230 C908 编译参数（与 SDK ai_demo 官方一致）
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=c908v -mabi=lp64d -mtune=c908 -mrvv-v0p10-compatible -mrvv-auto-vectorize -mcmodel=medany")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=c908v -mabi=lp64d -mtune=c908 -mrvv-v0p10-compatible -mrvv-auto-vectorize -mcmodel=medany")

# 在 sysroot 内搜索头文件和库
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
