set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# 设置编译器
set(CMAKE_C_COMPILER riscv64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-linux-gnu-g++)

# 设置编译选项
set(CMAKE_C_FLAGS_INIT "-march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
set(CMAKE_CXX_FLAGS_INIT "-march=rv64imafdcv -mabi=lp64d -mcmodel=medany -Wno-narrowing")