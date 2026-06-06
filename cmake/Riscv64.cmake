set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# 设置编译器
set(CMAKE_C_COMPILER riscv64-unknown-linux-musl-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-linux-musl-g++)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
