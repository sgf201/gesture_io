cmake_minimum_required(VERSION 3.22.1)
project(gesture_demo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置工具链
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/Riscv64.cmake)

# SDK路径设置 - 与SDK保持一致
if(NOT DEFINED SDK_RTSMART_SRC_DIR)
    set(SDK_RTSMART_SRC_DIR "/home/sgf/ws/k230_pangofly/src/rtsmart")
endif()

# 设置环境变量（与SDK的build_app.sh保持一致）
set(NNCASE_SRC_DIR "${SDK_RTSMART_SRC_DIR}/libs/nncase")
set(OPENCV_SRC_DIR "${SDK_RTSMART_SRC_DIR}/libs/opencv")
set(MPP_SRC_DIR "${SDK_RTSMART_SRC_DIR}/mpp")

# 设置链接脚本和静态链接
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/cmake/link.lds --static")

# 设置库路径
set(NNCASE_DIR "${NNCASE_SRC_DIR}/riscv64")
set(NNCASE_INCLUDE_DIR "${NNCASE_DIR}/nncase/include")
set(GSL_INCLUDE_DIR "${NNCASE_DIR}")
set(OPENCV_INCLUDE_DIR "${OPENCV_SRC_DIR}/include/opencv4")
set(RTSMART_HAL_INCLUDE_DIR "${SDK_RTSMART_SRC_DIR}/libs/rtsmart_hal/include")
set(MPP_INCLUDE_DIR "${MPP_SRC_DIR}/include")
set(MPP_COMM_INCLUDE_DIR "${MPP_SRC_DIR}/include/comm")
set(MPP_API_INCLUDE_DIR "${MPP_SRC_DIR}/userapps/api")
set(BSP_INCLUDE_DIR "${SDK_RTSMART_SRC_DIR}/rtsmart/kernel/bsp/maix3")

# 库文件路径
set(NNCASE_LIBS_DIR "${NNCASE_DIR}/nncase/lib")
set(RVV_LIBS_DIR "${NNCASE_DIR}/rvvlib")
set(OPENCV_LIBS_DIR "${OPENCV_SRC_DIR}/lib")
set(OPENCV_3RDPARTY_DIR "${OPENCV_SRC_DIR}/lib/opencv4/3rdparty")
set(RTSMART_HAL_LIBS_DIR "${SDK_RTSMART_SRC_DIR}/libs/rtsmart_hal/build/lib")
set(MPP_LIBS_DIR "${MPP_SRC_DIR}/userapps/lib")
set(RTTHREAD_LIBS_DIR "${SDK_RTSMART_SRC_DIR}/rtsmart/userapps/sdk/rt-thread/lib/risc-v/rv64")

# 包含common_files目录
set(common_files "${CMAKE_SOURCE_DIR}/common_files")
include_directories(${common_files})

# 设置common文件
set(common_files_src
    ${common_files}/ai_base.cc
    ${common_files}/ai_utils.cc
    ${common_files}/video_pipeline.cc
)

# 设置源文件
set(src main.cc hand_detection.cc hand_keypoint.cc)
set(all_src ${src} ${common_files_src})
set(bin gesture_demo.elf)

# 包含目录 - GSL必须在NNCASE之前
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${NNCASE_DIR})
include_directories(${GSL_INCLUDE_DIR})
include_directories(${NNCASE_INCLUDE_DIR})
include_directories(${NNCASE_INCLUDE_DIR}/nncase/runtime)
include_directories(${OPENCV_INCLUDE_DIR})
include_directories(${RTSMART_HAL_INCLUDE_DIR})
include_directories(${MPP_INCLUDE_DIR})
include_directories(${MPP_COMM_INCLUDE_DIR})
include_directories(${MPP_API_INCLUDE_DIR})
include_directories(${BSP_INCLUDE_DIR})

# 添加链接库路径
link_directories(${RVV_LIBS_DIR})
link_directories(${NNCASE_LIBS_DIR})
link_directories(${OPENCV_LIBS_DIR})
link_directories(${OPENCV_3RDPARTY_DIR})
link_directories(${RTSMART_HAL_LIBS_DIR})
link_directories(${MPP_LIBS_DIR})
link_directories(${RTTHREAD_LIBS_DIR})

add_executable(${bin} ${all_src})

# 链接库（与SDK中的demo完全一致）
target_link_libraries(${bin} -Wl,--start-group 
    rvv 
    Nncase.Runtime.Native 
    nncase.rt_modules.k230 
    functional_k230 
    sys 
    vicap 
    vb 
    isp 
    vo 
    connector 
    sensor 
    atomic 
    gsdma 
    rtthread 
    -Wl,--end-group)

target_link_libraries(${bin} -Wl,--start-group 
    opencv_imgcodecs 
    opencv_imgproc 
    opencv_core 
    zlib 
    libjpeg-turbo 
    libopenjp2 
    libpng 
    libtiff 
    libwebp 
    csi_cv 
    -Wl,--end-group)

install(TARGETS ${bin} DESTINATION bin)
